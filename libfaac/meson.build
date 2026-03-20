link_args = []
if host_machine.system() == 'windows' and meson.get_compiler('c').get_id() == 'gcc'
    # identifies mingw
    link_args += '-Wl,--add-stdcall-alias'
endif

if cc.get_id() == 'msvc'
  # MSVC doesn't have a direct 'hidden' attribute equivalent in C
  prod_visibility = '-DFAAC_INTERNAL=' 
else
  prod_visibility = '-DFAAC_INTERNAL=__attribute__((visibility("hidden")))'
endif

quantize_simd_libs = []

if config_h.get('HAVE_SSE2')
    sse2_flag = (cc.get_argument_syntax() == 'msvc') ? ['/arch:SSE2'] : ['-msse2']
    quantize_sse = static_library('quantize_sse', 'quantize_sse.c',
        include_directories: ['..', '../include'],
        c_args: c_args + sse2_flag + [prod_visibility],
        gnu_symbol_visibility: 'hidden',
        pic: true
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

libfaac = shared_library('faac', common_src,
    include_directories: ['..', '../include'],
    c_args: c_args + [prod_visibility],
    link_args: link_args,
    dependencies: [ libm ],
    link_with: quantize_simd_libs,
    vs_module_defs: 'libfaac.def',
    install: true,
    version: '0.0.0',
    gnu_symbol_visibility: 'hidden'
)

if get_option('tests')
    faac_static = static_library('faac_static',
        common_src,
        include_directories: ['..', '../include'],
        c_args: c_args + ['-DFAAC_INTERNAL=extern'],
        dependencies: [ libm ],
        link_with: quantize_simd_libs,
        pic: true
    )

    faac_internal = declare_dependency(
        link_with: faac_static,
        compile_args: ['-DFAAC_INTERNAL=extern'],
        include_directories: include_directories('.')
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
