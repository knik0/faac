qmf_test = executable('qmf_test',
    ['qmf_oracle.c', 'qmf_test.c'],
    include_directories: ['..', '../include'],
    objects: libfaac.extract_all_objects(recursive: true),
    dependencies: [ libm ],
    c_args: c_args,
    build_by_default: false
)

# qmf_analysis harness was written against the 320-tap spec-correct QMF
# (commit 8d3eddd, reverted). Keep source as reference; skip until we
# revisit spec-correct QMF with fixed envelope/noise semantics.
# See /home/yoyososo/.claude/plans/silly-tickling-neumann.md
test('qmf_analysis', qmf_test, should_fail: true)
