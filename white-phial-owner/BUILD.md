# White Phial Owner: additive update to merged traits v2.0

The native source remains in `../venom-harvester` and its previously built
1.1.0 DLL is preserved byte for byte. This update only adds plugin records,
Papyrus scripts, registration entries, one SEQ entry and a thumbnail.

Inputs:

- Exact `Biggie_Traits_Combined_Single_ESP_v2_0.zip` under `input/`. SHA-256 is
  checked by the builder.
- Uploaded White Phial Tweaks and Enhancements plugin at
  `../white-phial-menu/original/The White Phial - Tweaks and Enhancements.esp`
  and uploaded Requiem at `../upload/03-Requiem.esp`, for validation of external
  form bindings. These third-party plugin inputs are not vendored here.
- A Papyrus compiler, SKSE/base headers and compile-time interfaces to
  `PO3_SKSEFunctions.GetDoorDestination`, `MS12PostQuestScript` and
  `MS12WhitePhialScript`. Original installed mods provide the runtime classes;
  do not package stripped compile-time headers as runtime replacements.
- A Papyrus disassembler (verification currently expects
  `../fully-devoted/disassemble`) and ImageMagick for DDS conversion.
- The generated source image `art/White_Phial_Owner.png`. The source prompt
  is in `art/Artwork-Prompt.txt`; the production DDS is in the release ZIP.

Run from the repository root:

```sh
python3 white-phial-owner/build.py
papyrus compile -nocache -h white-phial-owner/headers -i white-phial-owner/papyrus -o white-phial-owner/compiled
```

Copy the four compiled PEX files to `staging/Scripts`. The builder places the
new PSC sources in `staging/Source/Scripts`. If editing scripts after building,
copy those updated sources too. The three dependency interfaces used for
compilation are only headers and are never shipped.

Convert the generated art with ImageMagick:

```sh
convert white-phial-owner/art/White_Phial_Owner.png -resize '1024x704^' -gravity center -extent 1024x704 -alpha on -define dds:compression=dxt5 -define dds:mipmaps=0 white-phial-owner/staging/Interface/TraitPics/Traits_WhitePhialOwnerAb.dds
python3 white-phial-owner/verify.py
python3 white-phial-owner/package.py <source-commit-sha>
```

The validator checks all prior records and runtime files for exact preservation,
the new perk's inventory condition, the supplied phial list and shop cell,
all new quest/alias bindings, compiled calls, SEQ and trait/texture registration.
It cannot test Skyrim runtime event ordering or gameplay behavior.

The phial refill service is `MS12PostQuest`, distinct from the two story quests
`MS12` and `MS12b`. Starting the service runs no quest fragments in the supplied
plugin (its fragment count is zero). The gift calls the original
`MS12WhitePhialScript.SetForRefill`, initializes `CurrentContainer`, and uses
the original healing liquid only when the service has no selected liquid.
No original phial script is overridden and no full-enchantment global is set.

Shop door discovery uses powerofthree's implementation, inspected at commit
`81e4c6b7b4912a3dea6e6be22c31c5e805f69503`:
[GetDoorDestination source](https://github.com/powerof3/PapyrusExtenderSSE/blob/81e4c6b7b4912a3dea6e6be22c31c5e805f69503/src/Papyrus/Functions/ObjectReference.cpp).
The function resolves the door's ExtraTeleport linked-door reference. Its
destination must belong to verified Skyrim cell `0001678C`, WindhelmWhitePhial.
Up to eight entrances receive aliases for immediate lock/activate events;
cell scans and crosshair events discover the relevant loaded references.
