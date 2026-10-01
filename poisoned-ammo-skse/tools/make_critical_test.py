"""Compile the actual critical wrapper, not a copied simulation of its logic."""
from pathlib import Path
import sys
root = Path(__file__).resolve().parents[1]
source = (root/'src/Precision.h').read_text()
start = source.index('    struct CriticalHook\n')
end = source.index('    inline void install(', start)
template = (root/'tests/critical_hook_test.cpp.in').read_text()
Path(sys.argv[1]).write_text(template.replace('@@CRITICAL_HOOK@@', source[start:end]))
