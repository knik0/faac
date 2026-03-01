link_args = []
if host_machine.system() == 'windows' and meson.get_compiler('c').get_id() == 'gcc'
    # identifies mingw
    link_args += '-Wl,--add-stdcall-alias'
endif


quantize_simd_libs = []

if (cpu_family in ['x86', 'x86_64'])
    sse2_flag = (cc.get_argument_syntax() == 'msvc') ? ['/arch:SSE2'] : ['-msse2']
    quantize_sse = static_library('quantize_sse', 'quantize_sse.c',
        include_directories: ['..', '../include'],
        c_args: c_args + sse2_flag
    )
    quantize_simd_libs += quantize_sse
endif

common_src = [
    'bitstream.c',
    'bitstream.h',
    'blockswitch.c',
    'blockswitch.h',
    'channels.c',
    'channels.h',
    'coder.h',
    'cpu_compute.c',
    'cpu_compute.h',
    'filtbank.c',
    'filtbank.h',
    'fft.c',
    'fft.h',
    'frame.c',
    'frame.h',
    'huff2.c',
    'huff2.h',
    'huffdata.c',
    'huffdata.h',
    'quantize.c',
    'quantize.h',
    'stereo.c',
    'stereo.h',
    'tns.c',
    'tns.h',
    'util.c',
    'util.h',
]

libfaac = library('faac', common_src,
    include_directories: ['..', '../include'],
    c_args: c_args,
    link_args: link_args,
    dependencies: [ libm ],
    link_with: quantize_simd_libs,
    vs_module_defs: 'libfaac.def',
    install: true
)

if get_option('drm')
    drm_src = [
        'kiss_fft/kiss_fftr.c',
        'kiss_fft/kiss_fft.c',
        'kiss_fft/kiss_fft.h'
    ]
    libfaac_drm = library('faac_drm', common_src + drm_src,
        include_directories: ['..', '../include'],
        c_args: c_args + ['-DDRM'],
        link_args: link_args,
        dependencies: [ libm ],
        link_with: quantize_simd_libs,
        vs_module_defs: 'libfaac.def',
        install: true
    )
endif

pkgconfig = import('pkgconfig')
pkgconfig.generate(
    libfaac,
    name: 'FAAC',
    description: 'Freeware Advanced Audio Coder',
    version: meson.project_version(),
    filebase: 'faac'
)
