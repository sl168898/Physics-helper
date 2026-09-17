# GildFlail direct SMP p4 — FSMP 3.2.1

Experimental player third-person physics for the supplied Lorerim GildFlail weapons.
The user confirmed the prior five-bone implementation works for one-handed flails.
p4 adds the two-bone Iron War Flail and four-bone Steel War Flail chains identified
in the user's supplied meshes. Five-bone one-handed rigs continue to work.

Each weapon instance gets its own physics system rooted at its own Flail_0.
The original GildFlailPhysics.xml is reused; FSMP skips missing bones and their
constraints for the shorter rigs. No dummy bones or duplicate weapon forms are
created, and two-handed weapons remain two-handed. No armor slot is used by
this helper. Existing legacy armor physics causes the helper to suspend to avoid
two simulations driving the same nodes.

Only source code is stored here. The private companion ESP remains unchanged
from p3 and removes the original armor-equipping scripts from all ten Lorerim
weapon records. No original meshes, XML, or companion ESP are published here.

Target: Skyrim 1.6.1170 / FSMP 3.2.1. p4 requires in-game testing. It does not add
first-person or NPC support. This DLL replaces FSMP's hdtsmp64.dll; do not install
it as a second DLL under a different name.

Portable regression tests:

    g++ -std=c++20 -Wall -Wextra -Werror -I source/src tests/rig_tests.cpp -o rig_tests
    ./rig_tests

Windows build: tools/build_windows.ps1, or the included GitHub Actions workflow.
Upstream FSMP and dependency revisions are pinned in the build recipe.
