project('faac', 'c', version: '1.31.1')

add_global_arguments('-DHAVE_CONFIG_H', language: 'c')

config_h = configuration_data()
config_h.set_quoted('PACKAGE', meson.project_name())
config_h.set_quoted('PACKAGE_VERSION', meson.project_version())

cc = meson.get_compiler('c')
foreach h: ['getopt.h', 'immintrin.h', 'stdint.h', 'sys/time.h', 'sys/types.h']
    config_h.set('HAVE_' + h.underscorify().to_upper(), cc.has_header(h))
endforeach


config_h.set('HAVE_STRCASECMP', cc.has_function('strcasestr'))


libm = cc.find_library('m', required: false)

cpu_family = target_machine.cpu_family()

doSse = cpu_family in ['x86', 'x86_64']
config_h.set('CPUSSE', doSse)

c_args = []
if cc.get_argument_syntax() == 'msvc'
    c_args += ['-DWIN32', '-DNDEBUG', '-D_WINDOWS', '-D_USRDLL', '-DLIBFAAC_DLL_EXPORTS']
else
    if doSse
        c_args += ['-msse2']
    endif
    c_args += ['-fvisibility=hidden']
endif

configure_file(
    output: 'config.h',
    configuration: config_h
)


subdir('docs')
subdir('include')
subdir('libfaac')

if get_option('frontend')
    subdir('frontend')
endif
