"""Native SVG realization of the approved Wheeler ammo silhouette preview."""
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parent
COLORS = {'none': '#DADDDD', 'fire': '#EE7755', 'frost': '#7DDBEF',
          'shock': '#B99AEA', 'poison': '#A5CE92'}
BODY = '#DADDDD'

def svg(bolt, head, coating):
    # Upright master geometry rotates toward the upper left. Both color
    # channels are separate filled paths; body and feather color never changes.
    if bolt:
        head_path = 'M64 17 L49 46 L59 42 L64 48 L69 42 L79 46 Z'
        body_path = 'M60 40 H68 V88 H60 Z M60 79 L49 86 L49 95 L56 92 L53 104 L61 99 L64 115 L67 99 L75 104 L72 92 L79 95 L79 86 L68 79 Z'
        dx, dy = 36, 53
    else:
        head_path = 'M64 -6 L51 26 L60 21 L64 30 L68 21 L77 26 Z'
        body_path = 'M61 19 H67 V110 H61 Z M61 101 L51 109 L50 119 L57 115 L53 128 L61 122 L64 134 L67 122 L75 128 L71 115 L78 119 L77 109 L67 101 Z'
        dx, dy = 25, 43
    drop = ''
    if coating != 'none':
        drop = f'  <path fill="{COLORS[coating]}" d="M{dx} {dy} C{dx-2} {dy+5} {dx-6} {dy+9} {dx-6} {dy+13} A6 6 0 0 0 {dx+6} {dy+13} C{dx+6} {dy+9} {dx+2} {dy+5} {dx} {dy} Z"/>\n'
    body_paths = ''.join(f'    <path fill="{BODY}" d="{part.strip()} Z"/>\n' for part in body_path.split('Z') if part.strip())
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="128" height="128" viewBox="0 0 128 128" data-wheeler-raster-size="2048">
  <g transform="rotate(-45 64 64)">
{body_paths}    <path fill="{COLORS[head]}" d="{head_path}"/>
  </g>
{drop}</svg>
'''

def main():
    output = ROOT / 'assets/ammo_silhouettes'
    output.mkdir(parents=True, exist_ok=True)
    count = 0
    for shape in ['arrow', 'bolt']:
        for head in COLORS:
            for coating in COLORS:
                if head == 'none' and coating != 'none':
                    continue
                (output / f'{shape}_{head}_{coating}.svg').write_text(svg(shape == 'bolt', head, coating))
                count += 1
    legacy = ROOT / 'assets/poisoned_ammo'
    legacy.mkdir(exist_ok=True)
    for shape in ['arrow', 'bolt']:
        shutil.copyfile(output / f'{shape}_poison_poison.svg', legacy / f'poisoned_{shape}.svg')
    print(f'Generated {count} transparent 2K-marked SVGs and 2 legacy aliases')

if __name__ == '__main__':
    main()
