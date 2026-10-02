"""Compile the exact production pump/queue methods with minimal engine doubles."""
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[1]
source = (root / 'src/CorpseExplosionRuntime.h').read_text()
start = source.index('        void pump(std::uint64_t generation) {')
end = source.index('        void update(float delta) {', start)
template = (root / 'tests/chain_pump_tests.cpp.in').read_text()
assert template.count('@@PRODUCTION_PUMP@@') == 1
Path(sys.argv[1]).write_text(template.replace('@@PRODUCTION_PUMP@@', source[start:end]))
