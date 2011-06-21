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
    ],
    libraries = [
        "rt"
    ],
    extra_compile_args = ["-g"],
    extra_link_args = ["-g"],
)

setup (name = 'pyllprof',
       version = '1.0',
       description = 'This is a demo package',
       ext_modules = [module1])


