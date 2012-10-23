#!/bin/bash
#convenience script, used by Makefile to enable easy cross-platform linking (Linux <> OS X):
#returns the library dependencies of MulticoreBSP for C on this platform
#uses a trick from user mhawke at stackoverflow.com
echo "int main(){}" | gcc -o /dev/null -x c - -lrt 2>/dev/null && echo "-pthread -lrt" || echo "-pthread"
