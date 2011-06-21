from distutils.core import setup, Extension

module1 = Extension(
    'pyllprof',
    sources = [
        'pyllprof.cpp',
        'call_tree.cpp',
        'class_table.cpp',
        'measurement.cpp',
        'server.cpp',
        'windows_support.cpp',
        'llprof.cpp',
    ],
    libraries = [
        "rt"
    ],
    extra_compile_args = ["-O3"],
    extra_link_args = ["-O3"],
)

setup (name = 'pyllprof',
       version = '1.0',
       description = 'This is a demo package',
       ext_modules = [module1])


