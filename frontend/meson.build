libiconv = cc.find_library('iconv', required: false)
has_iconv = cc.has_header('iconv.h') and cc.has_function('iconv_open', dependencies: [libiconv])

faac_c_args = []
if has_iconv
    faac_c_args += '-DHAVE_ICONV'
endif

libshell32 = cc.find_library('shell32', required: target_machine.system() == 'windows')

faac = executable('faac',
    ['main.c', 'input.c', 'mp4write.c', 'output.c', 'charset.c',
     'input.h', 'mp4write.h', 'output.h', 'charset.h'],
    include_directories: ['..', '../include'],
    c_args: faac_c_args,
    link_with: libfaac,
    dependencies: [libshell32, libiconv],
    install: true
)

if target_machine.system() == 'windows'
    windows = import('windows')
    resource_src = windows.compile_resources('faacgui.rc', 'icon.rc')
    faacgui = executable('faacgui',
        ['input.c', 'input.h', 'maingui.c', 'charset.c', 'charset.h', 'resource.h'] + resource_src,
        include_directories: ['..', '../include'],
        link_with: libfaac,
        win_subsystem: 'windows',
        install: true
    )
endif
