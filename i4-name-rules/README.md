# InventoryInjector name keyword compatibility v1.3

Modified I4 1.1.1, upstream commit 52cc4cb30f2ec09eba104606c8cdd17050c3a2bf.
Adds `{"contains":"weapon oil","ignoreCase":true,"anyOf":[]}` as a string-property match.
The empty `anyOf` must be retained: an unmodified I4 DLL interprets that as no match.
Empty or invalid contains specifications also match nothing in the modified DLL.

The combined mod filters to alchemy items with the Poison flag and assigns the oil
icon and `subTypeDisplay: "Weapon Oil"`. This changes presentation only. English
ASCII case folding is used; the phrase may occur anywhere in a longer UTF-8 name.
Existing exact-name/range/flag match semantics are unchanged.

Build with Windows 2022/Visual Studio 2022 x64:
`powershell -File tools/build_windows.ps1`
The script pins upstream I4, both submodules, vcpkg tooling, and the 2023-02-24
dependency registry compatible with upstream's 2023 CommonLib headers.
The CommonLib build-only patch requests Boost headers without asking CMake to
locate a nonexistent compiled stl_interfaces library; the dependency is header-only.
It creates `InventoryInjector_Name_Keywords_v1_3.zip` containing the DLL, modified
source, patch, licenses, test source, build script and build provenance.

This component is intended for the user's Steam Skyrim 1.6.1170 setup. Native build
and automated name-matching tests do not constitute an in-game test.
