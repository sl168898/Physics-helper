BIGGIE TRAITS -- THUMBNAIL HEADER FIX v1
Skald / Venom Harvester / Voice of Authority

Install this ZIP as a separate mod in Mod Organizer 2. Enable it BELOW your
combined traits package and any other trait thumbnail replacements in the
LEFT pane, so these three DDS files win their conflicts. Fully close Skyrim
and launch it again to reload the textures.

This is a texture-only overlay. It includes no ESP, scripts or DLL. Use it
with your current save; no new game, plugin load-order change or trait
reselection is needed. It works with the previous combined packages that
use these three thumbnail paths, including the four-ESP edition. The
separate full v2.1.1 package also contains this repair; use one installation
route, since you do not need the overlay on top of v2.1.1.

The original artwork is retained exactly. Only two DDS header fields in
each affected file were corrected:
- Compressed top-level image size: 4,108 -> 720,896 bytes.
- Unused RGB bit-count field: 32 -> 0, matching the original mod's DXT5 files.
The corrected 128-byte headers match the supplied original thumbnail mod.
The compressed image data and decoded pixels are byte-identical to before.

The corrupted striped display is consistent with these malformed headers.
Header checks, independent image decoding and packaging checks passed.
Skyrim itself was unavailable for testing; please reopen these three trait
cards in game after installing the overlay and restarting.
