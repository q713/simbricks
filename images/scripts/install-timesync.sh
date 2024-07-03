#!/bin/bash -eux

ls -l /bin/sh
cd /bin
rm sh
ln -s bash sh

apt-get update
apt-get -y install \
  autoconf2.69 \
  bison \
  build-essential \
  chrony \
  cmake \
  file \
  git \
  libncurses-dev \
  linuxptp \
  yarnpkg

# ln -s /usr/bin/yarnpkg /usr/bin/yarn
# for f in autoconf autoheader autom4te autoreconf autoscan autoupdate ifnames
# do
#   cp /usr/bin/${f}2.69 /usr/bin/${f}
# done
