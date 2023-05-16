/*
 * Copyright 2022 Max Planck Institute for Software Systems, and
 * National University of Singapore
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <cassert>

#include "analytics/spanner.h"
#include "util/exception.h"

bool HostSpanner::create_trace_starting_span (uint64_t parser_id)
{
  if (pending_host_call_span_ and is_client_)
  {
    // Note: this is to inform potential server hosts, that the trace is now
    // done, hence no new spans are added to it!
    if (not tracer_.mark_trace_as_done (
            pending_host_call_span_->get_trace_id ()))
    {
      std::cerr << "client could not mark trace as done" << std::endl;
    }
  }

  pending_host_call_span_ =
          tracer_.rergister_new_trace<host_call_span> (parser_id);
  if (not pending_host_call_span_)
  {
    std::cerr << "could not register new pending_host_call_span_";
    std::cerr << std::endl;
    return false;
  }

  found_transmit_ = false;
  found_receive_ = false;
  expected_xmits_ = 0;
  pci_msix_desc_addr_before_ = false;
  return true;
}

bool HostSpanner::handel_call (std::shared_ptr<Event> &event_ptr)
{
  assert(event_ptr and "event_ptr is null");

  if (not pending_host_call_span_ and
      not create_trace_starting_span (event_ptr->get_parser_ident ()))
  {
    // create a new pack that starts a trace
    return false;
  }

  if (pending_host_call_span_->add_to_span (event_ptr))
  {
    pci_msix_desc_addr_before_ =
            trace_environment::is_pci_msix_desc_addr (event_ptr);

    if (trace_environment::is_nw_interface_send (event_ptr))
    {
      ++expected_xmits_;
      found_transmit_ = true;
    } else if (trace_environment::is_nw_interface_receive (event_ptr))
    {
      found_receive_ = true;
    }
    return true;

  } else if (pending_host_call_span_->is_complete ())
  {
    // create a completely new trace
    if (found_receive_ and found_transmit_ and
        not create_trace_starting_span (event_ptr->get_parser_ident ()))
    {
      std::cerr << "found new syscall entry, could not allocate ";
      std::cerr << "pending_host_call_span_" << std::endl;
      return false;

      // get new pack for current trace
    } else
    {
      pending_host_call_span_ =
              tracer_.rergister_new_span_by_parent<host_call_span> (
                      pending_host_call_span_, event_ptr->get_parser_ident ());

      if (not pending_host_call_span_)
      {
        return false;
      }
    }

    if (not pending_host_call_span_ or
        not pending_host_call_span_->add_to_span (event_ptr))
    {
      std::cerr << "found new syscall entry, could not add "
                   "pending_host_call_span_"
                << std::endl;
      return false;
    }

    if (pending_host_call_span_->add_to_span (event_ptr))
    {
      return true;
    }
  }

  return false;
}

bool HostSpanner::handel_mmio (std::shared_ptr<concurrencpp::executor> resume_executor,
                               std::shared_ptr<Event> &event_ptr)
{
  assert(event_ptr and "event_ptr is null");

  if (not pending_host_mmio_span_)
  {
    // create a pack that belongs to the trace of the current host call span
    pending_host_mmio_span_ =
            tracer_.rergister_new_span_by_parent<host_mmio_span> (
                    pending_host_call_span_, event_ptr->get_parser_ident (),
                    pci_msix_desc_addr_before_);

    if (not pending_host_mmio_span_)
    {
      return false;
    }
  }

  if (pending_host_mmio_span_->add_to_span (event_ptr))
  {
    // as the nic receives his events before this span will
    // be completed, we indicate to the nic that a mmiow a.k.a send is expected
    if (is_type (event_ptr, EventType::HostMmioW_t) or
        is_type (event_ptr, EventType::HostMmioR_t))
    {
      if (not queue_.push (resume_executor, this->id_, expectation::mmio,
                           pending_host_mmio_span_).get())
      {
        std::cerr << "could not push to nic that mmio is expected"
                  << std::endl;
        // note: we will not return false as the span creation itself id work
      }
    }

    if (pending_host_mmio_span_->is_complete ())
    {
      // if it is a write after xmit, we inform the nic packer that we expect a
      // transmit
      if (pending_host_mmio_span_->is_write () and expected_xmits_ > 0 and
          not pending_host_mmio_span_->is_after_pci_msix_desc_addr ())
      {
        if (queue_.push (resume_executor, this->get_id (), expectation::tx,
                         pending_host_mmio_span_).get())
        {
          --expected_xmits_;
        } else
        {
          std::cerr
                  << "unable to inform nic spanner of mmio write that shall ";
          std::cerr << "cause a send" << std::endl;
        }
      }
      pending_host_mmio_span_ = nullptr;
    }
    return true;

  } else if (is_type (event_ptr, EventType::HostMmioW_t) and
             pending_host_mmio_span_->is_after_pci_msix_desc_addr ())
  {
    pending_host_mmio_span_->mark_as_done ();

    // the old one is done, we create a new one
    pending_host_mmio_span_ =
            tracer_.rergister_new_span_by_parent<host_mmio_span> (
                    pending_host_call_span_, event_ptr->get_parser_ident (),
                    pci_msix_desc_addr_before_);
    if (not pending_host_mmio_span_)
    {
      return false;
    }

    if (pending_host_mmio_span_->add_to_span (event_ptr))
    {
      return true;
    }
  }

  return false;
}

bool HostSpanner::handel_dma (std::shared_ptr<concurrencpp::executor> resume_executor,
                              std::shared_ptr<Event> &event_ptr)
{
  assert(event_ptr and "event_ptr is null");

  auto pending_dma =
          iterate_add_erase<host_dma_span> (pending_host_dma_spans_,
                                            event_ptr);
  if (pending_dma)
  {
    // if (pending_dma->is_complete()) {
    //   staged_host_dma_spans_.push_back(pending_dma);
    // }
    return true;
  }

  // when receiving a dma, we expect to get an context from the nic simulator,
  // hence poll this context blocking!!
  auto con = queue_.poll (resume_executor, this->id_).get();
  if (not is_expectation (con, expectation::dma))
  {
    std::cerr << "when polling for dma context, no dma context was fetched"
              << std::endl;
    return false;
  }
  assert(con->parent_span_ and "context has no parent span set");

  pending_dma = tracer_.rergister_new_span_by_parent<host_dma_span> (
          con->parent_span_, event_ptr->get_parser_ident ());
  if (not pending_dma)
  {
    return false;
  }

  if (pending_dma->add_to_span (event_ptr))
  {
    pending_host_dma_spans_.push_back (pending_dma);
    return true;
  }

  return false;
}

bool HostSpanner::handel_msix (std::shared_ptr<concurrencpp::executor> resume_executor,
                               std::shared_ptr<Event> &event_ptr)
{
  assert(event_ptr and "event_ptr is null");

  auto con = queue_.poll (resume_executor, this->id_).get();
  if (not is_expectation (con, expectation::msix))
  {
    std::cerr << "did not receive msix on context queue" << std::endl;
    return false;
  }

  auto host_msix_p = tracer_.rergister_new_span_by_parent<host_msix_span> (
          con->parent_span_, event_ptr->get_parser_ident ());

  if (not host_msix_p)
  {
    return false;
  }

  if (host_msix_p->add_to_span (event_ptr))
  {
    assert(host_msix_p->is_complete () and "host msix span is not complete");
    return true;
  }

  return false;
}

bool HostSpanner::handel_int (std::shared_ptr<Event> &event_ptr)
{
  assert(event_ptr and "event_ptr is null");

  if (not pending_host_int_span_)
  {
    pending_host_int_span_ =
            tracer_.rergister_new_span_by_parent<host_int_span> (
                    pending_host_call_span_, event_ptr->get_parser_ident ());
    if (not pending_host_int_span_)
    {
      return false;
    }
  }

  if (pending_host_int_span_->add_to_span (event_ptr))
  {
    return true;
  }

  return false;
}

concurrencpp::result<void> HostSpanner::consume (
        std::shared_ptr<concurrencpp::executor> resume_executor,
        std::shared_ptr<Channel<std::shared_ptr<Event>>> &src_chan)
{
  throw_if_empty (resume_executor, resume_executor_null);
  throw_if_empty (src_chan, channel_is_null);
  queue_.register_spanner (id_);

  std::shared_ptr<Event> event_ptr = nullptr;
  bool added = false;
  std::optional<std::shared_ptr<Event>> event_ptr_opt;

  for (event_ptr_opt = co_await src_chan->pop (resume_executor); event_ptr_opt.has_value ();
        event_ptr_opt = co_await src_chan->pop (resume_executor))
  {
    event_ptr = event_ptr_opt.value ();
    throw_if_empty (event_ptr, event_is_null);

    added = false;

    switch (event_ptr->get_type ())
    {
      case EventType::HostCall_t:
      {
        added = handel_call (event_ptr);
        break;
      }

      case EventType::HostMmioW_t:
      case EventType::HostMmioR_t:
      case EventType::HostMmioImRespPoW_t:
      case EventType::HostMmioCW_t:
      case EventType::HostMmioCR_t:
      {
        added = handel_mmio (resume_executor, event_ptr);
        break;
      }

      case EventType::HostDmaW_t:
      case EventType::HostDmaR_t:
      case EventType::HostDmaC_t:
      {
        added = handel_dma (resume_executor, event_ptr);
        break;
      }

      case EventType::HostMsiX_t:
      {
        added = handel_msix (resume_executor, event_ptr);
        break;
      }

      case EventType::HostPostInt_t:
      case EventType::HostClearInt_t:
      {
        added = handel_int (event_ptr);
        break;
      }

      default:
        std::cout << "encountered non expected event ";
        std::cout << *event_ptr << std::endl;
        added = false;
        break;
    }

    if (not added)
    {
      std::cout << "found event that could not be added to a pack: "
                << *event_ptr << std::endl;
    }
  }

  co_return;
}
