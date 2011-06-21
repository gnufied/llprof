
#include "Python.h"
#include "frameobject.h"
#include "code.h"
#include "object.h"
#include <iostream>
#include <string>
using namespace std;

#include "measurement.h"


static PyObject * minit(PyObject *self, PyObject *args)
{
    printf("Hello World!weweerwerwe!\n");
    Py_RETURN_NONE;
}


int pyllprof_tracefunc(PyObject *obj, PyFrameObject *frame, int what, PyObject *arg)
{
    switch(what)
    {
    case PyTrace_CALL:
        llprof_call_handler((nameid_t)frame->f_code, NULL);
        break;
        
    case PyTrace_EXCEPTION:
    case PyTrace_RETURN:
        llprof_return_handler();
        break;
        
    }
    return 0;
}

char* wstr_to_str(Py_UNICODE *s)
{
    char *buf = new char[256];
    int i = 0;
    for(; i < 255; i++)
    {
        buf[i] = s[i];
        if(!s[i])
            return buf;
    }
    buf[i] = 0;
    return buf;
}

const char *python_name_func(nameid_t nameid, void *p)
{

    if(!nameid)
        return "(null)";
    PyCodeObject *f_code = (PyCodeObject *)nameid;
    Py_ssize_t sz;
    Py_UNICODE* wstr = PyUnicode_AS_UNICODE(f_code->co_name);
    
    char *name = wstr_to_str(wstr);
    return name;

}

static PyMethodDef ext_methods[] = {
    {"__init__", minit, METH_VARARGS, "Initialize"},
    {NULL, NULL}
};

static struct PyModuleDef moduledef = {
        PyModuleDef_HEAD_INIT,
        "pyllprof",
        NULL,
        0,
        ext_methods,
        NULL,
        NULL,
        NULL,
        NULL
};


PyMODINIT_FUNC
PyInit_pyllprof(void)
{
    PyObject *module = PyModule_Create(&moduledef);

    llprof_set_time_func(get_time_now_nsec);
    llprof_set_name_func(python_name_func);
    llprof_init();
    PyEval_SetProfile(pyllprof_tracefunc, NULL);
    
    return module;
}
