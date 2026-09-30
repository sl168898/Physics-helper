"""Compile the production impact thunk against minimal engine test doubles."""
import pathlib
import sys

root = pathlib.Path(__file__).resolve().parents[1]
source = (root / 'src/main.cpp').read_text(encoding='utf-8')
start = source.index('    struct ImpactHook\n')
end = source.index('    struct ProcessHook\n', start)
template = (root / 'tests/impact_abi_test.cpp.in').read_text(encoding='utf-8')
assert template.count('@@PRODUCTION_IMPACT_HOOK@@') == 1
pathlib.Path(sys.argv[1]).write_text(template.replace('@@PRODUCTION_IMPACT_HOOK@@', source[start:end]), encoding='utf-8')
