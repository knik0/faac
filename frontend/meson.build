faac = executable('faac',
    ['main.c', 'input.c', 'mp4write.c', 'input.h', 'mp4write.h'],
    include_directories: ['..', '../include'],
    link_with: libfaac,
    install: true
)

if target_machine.system() == 'windows'
    windows = import('windows')
    resource_src = windows.compile_resources('faacgui.rc', 'icon.rc')
    faacgui = executable('faacgui',
        ['input.c', 'input.h', 'maingui.c', 'resource.h'] + resource_src,
        include_directories: ['..', '../include'],
        link_with: libfaac,
        install: true
    )
endif
