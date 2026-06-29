# Native MSVC has no <getopt.h>; pull a permissive getopt_long in at build
# time. Systems whose C library ships <getopt.h> (Linux, macOS, MSYS2) skip
# the subproject entirely.
getopt_dep = []
if not meson.get_compiler('c').has_header('getopt.h')
    getopt_dep = subproject('wingetopt').get_variable('getopt_dep')
endif

faac = executable('faac',
    ['main.c', 'input.c', 'mp4write.c', 'input.h', 'mp4write.h'],
    include_directories: ['..', '../include'],
    link_with: libfaac,
    dependencies: getopt_dep,
    install: true
)

if target_machine.system() == 'windows'
    windows = import('windows')
    resource_src = windows.compile_resources('faacgui.rc', 'icon.rc')
    faacgui = executable('faacgui',
        ['input.c', 'input.h', 'maingui.c', 'resource.h'] + resource_src,
        include_directories: ['..', '../include'],
        link_with: libfaac,
        win_subsystem: 'windows',
        install: true
    )
endif
