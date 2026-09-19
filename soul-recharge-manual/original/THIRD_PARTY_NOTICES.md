# Third-party notices

This package contains a native plugin built with the components below. The links are the upstream project or distribution page. The short copyright lines are notice excerpts; the upstream license text and terms control.

| Component | Upstream URL | License | Copyright notice excerpt |
|---|---|---|---|
| CommonLibSSE-NG | https://github.com/CharmedBaryon/CommonLibSSE-NG | MIT License | `Copyright (c) 2018 Ryan-rsm-McKenzie` |
| Address Library for SKSE Plugins | https://www.nexusmods.com/skyrimspecialedition/mods/32444 | MIT License | `Copyright notice and contributors are credited by the upstream project.` |
| SKSE64 | https://skse.silverlock.org/ | GNU General Public License v3.0 | `Copyright (C) SKSE Team and contributors.` |
| SkyUI | https://www.nexusmods.com/skyrimspecialedition/mods/3863 | GNU General Public License v3.0 | `Copyright (C) SkyUI Team.` |
| DirectXMath | https://github.com/microsoft/DirectXMath | MIT License | `Copyright (c) Microsoft Corporation.` |
| DirectXTK | https://github.com/microsoft/DirectXTK | MIT License | `Copyright (c) Microsoft Corporation.` |
| fmt | https://github.com/fmtlib/fmt | MIT License | `Copyright (c) 2012-present, Victor Zverovich.` |
| rapidcsv | https://github.com/d99kris/rapidcsv | BSD 3-Clause License | `Copyright (c) 2017 d99kris.` |
| rsm-binary-io | https://github.com/alandtse/rsm-binary-io | MIT License | `Copyright notice is retained by the upstream project.` |
| SimpleIni | https://github.com/brofield/simpleini | MIT License | `Copyright (c) 2006-2015 Brodie Thiesfield.` |
| spdlog | https://github.com/gabime/spdlog | MIT License | `Copyright (c) 2015 Gabi Melman.` |
| Xbyak | https://github.com/herumi/xbyak | BSD 3-Clause License | `Copyright (c) 2012-2023 M. Takahashi.` |

CommonLibSSE-NG is included as a static build dependency. The other native-library entries are selected by `vcpkg.json` and the CommonLibSSE-NG CMake configuration; some are header-only or transitive build dependencies. The runtime requirements SKSE, Address Library, and SkyUI are not redistributed in this archive.

`brotli` is not listed in the package's inspected dependency manifest or CMake configuration and is not claimed as a dependency of this archive.

The original mod code and documentation remain under the MIT License in `LICENSE`.
