#!/bin/bash
set -e

cp -av llprofcommon/* pyllprof/
cd pyllprof

python setup.py build



