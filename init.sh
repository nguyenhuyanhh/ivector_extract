#!/bin/bash

# client: make
cd src
make

# server: if code does not exist or is old, update code
ivectorbin="$KALDI_ROOT/src/ivectorbin"
if [ ! -f $ivectorbin/ivector-extract-server.cc ] ; then
    cp ivector-extract-server.cc $ivectorbin
elif ! cmp -s $ivectorbin/ivector-extract-server.cc ivector-extract-server.cc ; then
    cp ivector-extract-server.cc $ivectorbin
fi

# server: update Makefile and make
cd $ivectorbin
cp Makefile Makefile.bak
if ! grep -q "ivector-extract-server" Makefile.bak ; then
    awk '/ivector-normalize-length/ { print; print "\t\tivector-extract-server \\"; next }1' Makefile.bak > Makefile
fi
make
