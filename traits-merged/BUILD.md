# Single-plugin edition of the four custom trait addons

This directory contains the typed merger, binding validator, updated Papyrus
sources and packaging tool. The native source is in `../venom-harvester` on this
branch. `README.txt` contains installation and save compatibility instructions.

Inputs are the exact delivered `Biggie_Traits_Combined_v1_6.zip` (the merger checks
its SHA-256), a Papyrus compiler with the original dependency script headers,
and a Papyrus disassembler. These mod assets and external tools are not vendored
here. Put the baseline ZIP under `../outputs/`.

1. Run `python3 traits-merged/merge_traits.py` from the repository root. This
   writes the merged plugin and source into `traits-merged/staging` and records
   the mapping of every typed FormID reference. It deliberately copies the
   baseline compiled scripts, so compilation in the next step is mandatory.
2. Compile `papyrus/FD_PowerGrantAlias.psc` and
   `papyrus/WSN_TrackerQuest_Quest.psc` against the original dependency headers.
   Copy the resulting PEX files into `staging/Scripts`. All other PEX files and
   the four thumbnails stay unchanged.
3. The recorded validation used Caprica's `papyrus compile -nocache -h headers
   -i papyrus -o compiled` from the trait directory. For disassembly comparisons,
   `verify_merge.py` expects `../fully-devoted/disassemble` and the original
   extracted scripts at `../venom-harvester/staging/Scripts`; adjust these
   local tool/input paths if your build environment differs.
4. Run `python3 traits-merged/verify_merge.py`. It checks all four registrations,
   typed record bindings, quest alias/SEQ, untouched assets and both compiled
   scripts' instruction streams after reversing only the lookup literal edits.
5. Build the native plugin with the existing Windows GitHub Actions workflow on
   this branch. Download its actual `Venom_Harvester_SKSE_v1_1.zip` artifact.
6. Run `python3 traits-merged/package.py <native-zip> <native-source-commit>
   <workflow-run-id> <trait-source-commit>`. The packager validates native source
   hashes and DLL structure, preserves artwork credits, and creates the mod ZIP
   with a complete file hash manifest. It expects the recorded
   `script-validation.json` beside the merger. No Skyrim runtime is available
   in the build environment; in-game validation remains separate.

The shipped edition uses native build commit
`02ba0092ee909b9e954ad8079bf0d67b34ab0dae` and Windows workflow run `35304300204`.
The GitHub artifact digest is recorded in the package manifest. Existing
four-plugin saves are not migrated by these tools.
