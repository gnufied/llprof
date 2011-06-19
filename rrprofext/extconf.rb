
require 'mkmf'


try_link("../llprofcommon/call_tree.cpp")
have_header("../llprofcommon/call_tree.h")

create_makefile('rrprof')
