#!/bin/bash

# stop on errors
set -e

if [ "`id -nu`" != "root" ]; then
  echo "Must be called as root"
  exit 1
fi

if which dnf 2>/dev/null >/dev/null; then
  FLAGS="-y"
  dnf $FLAGS install cmake gcc-c++ git protobuf-compiler gtkmm24-devel
fi

if which apt-get 2>/dev/null >/dev/null; then
  FLAGS="-qq -y"
  apt-get $FLAGS install cmake g++ git libgtkmm-2.4-dev libprotobuf-dev protobuf-compiler
fi

if which emerge 2>/dev/null >/dev/null; then
  emerge dev-cpp/gtkmm:2.4 dev-libs/protobuf dev-vcs/git
fi
