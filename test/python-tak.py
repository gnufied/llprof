#!/usr/bin/env python3
import timeit

def tak(x, y, z):
    if x <= y:
        return y
    else:
        return tak(tak(x-1,y,z),tak(y-1,z,x),tak(z-1,x,y))


def main():
    def f():
        return tak(12,6,0)
    print("Start:")
    for i in range(6):
        t = timeit.Timer(stmt = f)
        print("Time", i, "=", t.timeit(number = 1))

mode = "n"
if mode == "c":
	import cProfile
	cProfile.run("main()")
elif mode == "ll":
	import pyllprof
	main()
else:
	print("Without profiler")
	main()

