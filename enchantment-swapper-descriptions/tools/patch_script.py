"""Apply the private script edit to a user-supplied Enchantment Swapper source."""
import argparse
import hashlib
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('original', type=Path)
parser.add_argument('output', type=Path)
args = parser.parse_args()
source = args.original.read_text(encoding='utf-8-sig')
anchor = '\tUnenchanted.SetItemCharge(Enchanted.GetItemCharge())\n'
if source.count(anchor) != 1 or 'ESD_Native.RememberTransfer' in source:
    raise SystemExit('Unrecognized or already patched source; refusing to edit it.')
replacement = anchor + '''\t; Keep custom item descriptions attached to this transferred enchantment.
\tif SKSE.GetPluginVersion("EnchantmentSwapperDescriptions") >= 0
\t\tESD_Native.RememberTransfer(Enchanted, Unenchanted, enchanting)
\tendif
'''
result = source.replace(anchor, replacement)
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(result, encoding='utf-8', newline='\n')
print(f'Patched {args.output.name}; SHA256 {hashlib.sha256(result.encode()).hexdigest()}')
