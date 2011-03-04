#ifndef RRPROF_CLASSTBL_H
#define RRPROF_CLASSTBL_H

#include <ruby/ruby.h>

void InitClassTbl();
const char * AddClassName(VALUE klass);

const char * GetClassName(VALUE klass);

#endif