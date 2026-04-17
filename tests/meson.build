qmf_test = executable('qmf_test',
    ['qmf_oracle.c', 'qmf_test.c'],
    include_directories: ['..', '../include'],
    objects: libfaac.extract_all_objects(recursive: true),
    dependencies: [ libm ],
    c_args: c_args,
    build_by_default: false
)

test('qmf_analysis', qmf_test)
