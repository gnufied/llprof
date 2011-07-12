
#include "Python.h"
#include "frameobject.h"
#include "code.h"
#include "object.h"
#include <iostream>
#include <string>
using namespace std;

#include "llprof.h"


pthread_key_t g_thread_flag_key;

int pyllprof_tracefunc(PyObject *obj, PyFrameObject *frame, int what, PyObject *arg);


void pyllprof_init()
{
    pthread_key_create(&g_thread_flag_key, NULL);

}

static PyObject * minit(PyObject *self, PyObject *args)
{
    Py_RETURN_NONE;
}


static PyObject * pyllprof_begin_profile(PyObject *self, PyObject *args)
{
    int *counter = (int *)pthread_getspecific(g_thread_flag_key);
    if(!counter)
    {
        counter = new int(0);
        pthread_setspecific(g_thread_flag_key, counter);
        PyEval_SetProfile(pyllprof_tracefunc, NULL);
    }
    (*counter)++;
    
    
    int ok;
    char *s;
    ok = PyArg_ParseTuple(args, "s", &s);
    llprof_call_handler(HashStr(s), s);

    Py_RETURN_NONE;
}



static PyObject * pyllprof_end_profile(PyObject *self, PyObject *args)
{
    int *counter = (int *)pthread_getspecific(g_thread_flag_key);
    assert(counter);
    (*counter)--;
    assert((*counter) >= 0);

    llprof_return_handler();
    Py_RETURN_NONE;
}



int pyllprof_tracefunc(PyObject *obj, PyFrameObject *frame, int what, PyObject *arg)
{
    int *counter = (int *)pthread_getspecific(g_thread_flag_key);
    if(*counter == 0)
        return 0;
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

#if PY_MAJOR_VERSION >= 3
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

const char *get_f_code_name(PyCodeObject *f_code)
{
    Py_ssize_t sz;
    Py_UNICODE* wstr = PyUnicode_AS_UNICODE(f_code->co_name);
    char *name = wstr_to_str(wstr);
    return name;
}

#else

const char *get_f_code_name(PyCodeObject *f_code)
{
    return PyString_AS_STRING(f_code->co_name);
}

#endif

const char *python_name_func(nameid_t nameid, void *p)
{
    if(!nameid)
        return "(null)";
    if(p)
        return (char *)p;

    return get_f_code_name((PyCodeObject *)nameid);
}


static PyMethodDef pyllprof_methods[] = {
    {"__init__", minit, METH_VARARGS, "Initialize"},
    {"begin_profile", pyllprof_begin_profile, METH_VARARGS, "Begin profile"},
    {"end_profile", pyllprof_end_profile, METH_VARARGS, "End profile"},
    {NULL, NULL}
};

#if PY_MAJOR_VERSION >= 3

static struct PyModuleDef moduledef = {
        PyModuleDef_HEAD_INIT,
        "pyllprof",
        NULL,
        0,
        pyllprof_methods,
        NULL,
        NULL,
        NULL,
        NULL
};


PyMODINIT_FUNC
PyInit_pyllprof(void)
#else
extern "C" void
initpyllprof(void)
#endif
{
#if PY_MAJOR_VERSION >= 3
    PyObject *module = PyModule_Create(&moduledef);
#else
    PyObject *module = Py_InitModule("pyllprof", pyllprof_methods);
#endif

    llprof_set_time_func(get_time_now_nsec);
    llprof_set_name_func(python_name_func);
    llprof_init();
    pyllprof_init();
    
    
#if PY_MAJOR_VERSION >= 3
    return module;
#endif
}
