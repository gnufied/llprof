
import sys
sys.path.append('../pyllprof')
import pydoc
import sys
import os
import pyllprof

os.mkdir("pydocs")
os.chdir("pydocs")

pydoc.writedocs("/usr/lib/python3.1")

