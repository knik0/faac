test_inc = include_directories(
    '../include',
    '../libfaac',
    '..'
)

# Internal component tests link against 'faac_internal' to verify functions
# that are normally static in production but promoted for testing/LTO.
internal_tests = [
    'util',
    'bitstream',
    'fft',
    'channels',
    'tns',
    'mdct',
    'huffman',
    'blockswitch',
    'quantize'
]

foreach t : internal_tests
    exe = executable('test_' + t, 'test_' + t + '.c',
        include_directories: test_inc,
        dependencies: [ libm, faac_internal ]
    )
    test(t, exe)
endforeach

# Public API test links against the official shared library
test_api = executable('test_api', 'test_api.c',
    include_directories: test_inc,
    dependencies: [ libm ],
    link_with: [ libfaac ]
)
test('api', test_api)
