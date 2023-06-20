# syntax=docker/dockerfile:1
FROM ubuntu:22.04

ARG llvmver=14
ARG DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Berlin

RUN apt-get update && \
    apt-get install -yq \
      wget \
      curl \
      unzip \
      git \
      make \
      cmake \
      apt-utils \
      autoconf \
      xz-utils  \
      build-essential  \
      software-properties-common \
      apt-transport-https \
      ca-certificates \
      libgtest-dev \
      libbenchmark-dev \
      ninja-build \
      protobuf-compiler \
      libcurl4-openssl-dev \
      clang-$llvmver \
      lldb-$llvmver && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*;

WORKDIR /usr/src
COPY . /usr/src

ENV CC clang-$llvmver
ENV CXX clang++-$llvmver

RUN cmake -B build-docker -DWITH_OTLP_HTTP=ON -DBUILD_TESTING=OFF -G Ninja . && \
    cd build-docker && \
    ninja

# TODO: execute the actual tracing utility
CMD [ "/bin/bash" ]
