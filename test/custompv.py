#!/usr/bin/env python
import sys
import time

ntest = 5
if len(sys.argv) >= 2:
    ntest = int(sys.argv[1])

class TestStrm:
    def write(self, msg):
        n = len([c for c in msg if c == 'a'])
        pyllprof.icl(2, n)
        print >>sys.__stdout__, msg,

sys.stdout = TestStrm()


def pen():
    print "This is a pen."

def about_ruby():
    print "A dynamic, open source programming language with a focus on simplicity and productivity."

def many_as():
    print "aaaaaaaaa"

def no_a():
    print "zzzzzz"

def testfunc():
    pen()
    about_ruby()
    many_as()
    no_a()
    

def main():
    def f():
        return tak(12,6,0)
    print("Start:")

    for i in range(1):
        testfunc()
        print "Sleeping"
        time.sleep(100)



mode = "ll"
if mode == "c":
    import cProfile
    cProfile.run("main()")
elif mode == "ll":
    import pyllprof
    pyllprof.begin_profile("main")
    main()
    pyllprof.end_profile()
else:
    print("Without profiler")
    main()

