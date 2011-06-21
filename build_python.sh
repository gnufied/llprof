#!/bin/bash
set -e

cp -av llprofcommon/* pyllprof/
cd pyllprof

python3 setup.py build

cp build/lib.linux-x86_64-3.1/pyllprof.so ./


