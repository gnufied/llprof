#!/bin/bash
set -e

cp -av llprofcommon/* pyllprof/
cd pyllprof

python3 setup.py build



