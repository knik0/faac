project('wingetopt', 'c', license: 'BSD-2-Clause')

# wingetopt is a permissive (NetBSD-derived) getopt/getopt_long port for
# platforms whose C library lacks <getopt.h> (native MSVC). Its header is
# named getopt.h, so adding src/ to the include path lets consumers keep
# using #include <getopt.h> unchanged. The name and location of src/getopt.h
# must stay stable; consumers depend on it directly.
inc = include_directories('src')

wingetopt_lib = static_library('wingetopt',
    'src/getopt.c',
    include_directories: inc,
)

getopt_dep = declare_dependency(
    include_directories: inc,
    link_with: wingetopt_lib,
)
