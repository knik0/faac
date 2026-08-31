project('faac', 'c',
    version: '2.1.0',
    default_options: [
        'default_library=both',
        'buildtype=release',
        'warning_level=3',
        'c_std=gnu11,c11',
        'b_lto=true'
    ])

cc = meson.get_compiler('c')
if cc.get_argument_syntax() == 'msvc'
    # _s variants buy nothing here and aren't portable; suppress instead.
    add_global_arguments('-D_CRT_SECURE_NO_WARNINGS', language: 'c')
endif

config_h = configuration_data()
config_h.set_quoted('PACKAGE', meson.project_name())
config_h.set_quoted('PACKAGE_VERSION', meson.project_version())

foreach h: ['getopt.h', 'immintrin.h']
    config_h.set('HAVE_' + h.underscorify().to_upper(), cc.has_header(h))
endforeach

config_h.set('WORDS_BIGENDIAN', host_machine.endian() == 'big')


libm = cc.find_library('m', required: false)

cpu_family = target_machine.cpu_family()

config_h.set('HAVE_SSE2', (cpu_family in ['x86', 'x86_64']) and config_h.get('HAVE_IMMINTRIN_H'))

config_h.set('FAAC_SBR_DECIMATION', get_option('sbr-decimation'))

config_h.set('MAX_CHANNELS', get_option('max-channels'))

c_args = []
if cc.get_argument_syntax() == 'msvc'
    c_args += ['-DWIN32', '-DNDEBUG', '-D_WINDOWS', '-D_USRDLL', '-DLIBFAAC_DLL_EXPORTS']
endif

configure_file(
    output: 'config.h',
    configuration: config_h
)

# Force config.h into every TU instead of relying on each file's own include.
if cc.get_argument_syntax() == 'msvc'
    add_project_arguments('/FI' + meson.current_build_dir() / 'config.h', language: 'c')
else
    add_project_arguments('-include', meson.current_build_dir() / 'config.h', language: 'c')
endif


subdir('docs')
subdir('include')
subdir('libfaac')

if get_option('frontend')
    subdir('frontend')
endif
