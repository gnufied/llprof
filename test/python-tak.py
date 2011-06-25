#!/usr/bin/env python3
import sys
# sys.path.append('../pyllprof')

import timeit

ntest = 5
if len(sys.argv) >= 2:
    ntest = int(sys.argv[1])
    


def tak(x, y, z):
    if x <= y:
        return y
    else:
        return tak(tak(x-1,y,z),tak(y-1,z,x),tak(z-1,x,y))


def main():
    def f():
        return tak(12,6,0)
    print("Start:")
    for i in range(ntest):
        t = timeit.Timer(stmt = f)
        print("Time", i, "=", t.timeit(number = 1))

mode = "ll"
if mode == "c":
	import cProfile
	cProfile.run("main()")
elif mode == "ll":
	import pyllprof
	main()
else:
	print("Without profiler")
	main()

