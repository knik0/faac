link_args = []
if host_machine.system() == 'windows' and meson.get_compiler('c').get_id() == 'gcc'
    # identifies mingw
    link_args += '-Wl,--add-stdcall-alias'
endif


quantize_simd_libs = []

if config_h.get('HAVE_SSE2')
    sse2_flag = (cc.get_argument_syntax() == 'msvc') ? ['/arch:SSE2'] : ['-msse2']
    quantize_sse = static_library('quantize_sse', 'quantize_sse.c',
        include_directories: ['..', '../include'],
        c_args: c_args + sse2_flag,
        gnu_symbol_visibility: 'hidden'
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

libfaac = disabler()

common_args = {
    'include_directories' : ['..', '../include'],
    'c_args' : c_args,
    'link_args' : link_args,
    'dependencies' : [libm],
    'link_with' : quantize_simd_libs,
    'install' : true,
    'gnu_symbol_visibility' : 'hidden',
}

default_lib = get_option('default_library')

# LTO on the static archive ships unportable compiler bytecode, so keep it off.
if default_lib == 'static' or default_lib == 'both'
    libfaac_static = static_library(
        'faac',
        common_src,
        kwargs : common_args + {
          'override_options' : ['b_lto=false'],
        },
    )
endif

if default_lib == 'shared' or default_lib == 'both'
    libfaac_shared = shared_library(
        'faac',
        common_src,
        kwargs : common_args + {
          'version' : '0.0.0',
          'vs_module_defs' : 'libfaac.def',
          'override_options' : ['b_lto=true'],
        },
    )
endif

# Prefer the LTO shared library for the frontend and pkgconfig.
if default_lib == 'static'
    libfaac = libfaac_static
else
    libfaac = libfaac_shared
endif

pkgconfig = import('pkgconfig')
pkgconfig.generate(
    libfaac,
    name: 'FAAC',
    description: 'Freeware Advanced Audio Coder',
    version: meson.project_version(),
    filebase: 'faac'
)
