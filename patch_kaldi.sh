#!/bin/bash

KALDI_ROOT=$1
ivectorbin="$KALDI_ROOT/src/ivectorbin"

# if code does not exist or is old, update code
if [ ! -f $ivectorbin/ivector-extract-server.cc ] ; then
    cp ./src/ivector-extract-server.cc $ivectorbin
elif ! cmp -s $ivectorbin/ivector-extract-server.cc ./src/ivector-extract-server.cc ; then
    cp ./src/ivector-extract-server.cc $ivectorbin
fi

# update Makefile and make
cd $ivectorbin
cp Makefile Makefile.bak
if ! grep -q "ivector-extract-server" Makefile.bak ; then
    awk '/ivector-normalize-length/ { print; print "\t\tivector-extract-server \\"; next }1' Makefile.bak > Makefile
fi
make
