# Link’s Awakening for PS5

**Installed from ProsperoStore?** [Use the game-specific setup](INSTALL.md#already-installed-from-prosperostore): choose your game files and an output location; it prepares and checks a ready-to-copy folder automatically. Download **PS5-Install-Helper-v1.2.0.zip** from this game's release. Python 3.9+ is required; game conversion/module preparation requirements are listed in the guide.

**[Start here: installation, USB/offline copying, Wi-Fi FTP and game-file locations](INSTALL.md).**
M.2, Ethernet and PS5 Upload are optional. The guide also explains recovery after
store updates; older packaged instructions may describe the developer's M.2 setup.

Native PS5 port of the 2019 Switch version of **The Legend of Zelda: Link’s Awakening**, based on [LAS Recomp](https://github.com/peiikai/LASRecomp). Title ID: **PPSA99670**.

The PS5 port uses Mihawk’s Vulkan foundation and the LAS Recomp NVN-to-Vulkan renderer. It runs recompiled game code with a native runtime. You must supply your own **Switch version 1.0.0** game dump and keys. Game files, keys, saves and generated game code are not distributed with this project. Update files are not supported by this build.

[Download v0.1.0](https://github.com/mshivam019/la-ps5/releases/tag/v0.1.0) · [PS5 setup](ps5/README.md) · [Build from source](ps5/BUILD.md)

## Features

- Native homebrew title **Link’s Awakening** (`PPSA99670`), launched from the PS5 home screen.
- 1920×1080 rendering with 3840×2160 FSR1-upscaled output.
- DualSense input, PlayStation button textures, native audio and movie playback.
- Controller-operated launcher and touchpad settings menu.
- Background shader compilation with persistent caches.
- Saves, settings and caches stored alongside the title.

## Requirements

- A homebrew-capable PS5 with native folder-title support and a compatible registration/mounting setup.
- Your own **Switch version 1.0.0** base-game dump and keys.
- A computer for preparing your game files and enough free space at your selected installation location.

## Installation

1. Install `PPSA99670.zip` from ProsperoStore/the release, or unpack the
   precompiled app for manual installation.
2. Use the matching Linux/WSL2 setup bundle on your own Switch v1.0.0 base NSP
   and keys to generate `extracted/` (including `game.la`, ExeFS and RomFS). The
   current generator still requires Python 3.11+, LLVM and about35GB free; the file
   helper does not replace this step or add native Windows/macOS generation.
3. With the game closed, copy **extracted/** into the installed `PPSA99670` folder.
   Use USB/file-manager copying, Wi-Fi FTP, Ethernet FTP or any preferred method.
   The destination is your loader/store's installed folder, not a fixed M.2 path.
4. Preserve saves/settings. A new manual app requires mounting/registration with
   your existing native-title loader; then start its home-screen tile.

[INSTALL.md](INSTALL.md) explains storage, offline transfer, the separate prepare
and transfer helpers, and game-file recovery after store updates. The original
game-module converter and file-transfer step are independent; preparing files
never requires a console connection.

## Controls

| Control | Action |
| --- | --- |
| Cross | A / confirm |
| Circle | B / back |
| Triangle | X |
| Square | Y |
| Options | Plus / game menu |
| L3 + R3 | Minus |
| Touchpad | Runtime settings menu |

## Repository layout

```text
runtime/       Native runtime, graphics, audio and UI
ps5/           Platform integration, metadata, build and setup tools
third_party/   Vendored libraries and notices
tools/         Game extraction and code generation
```

## Credits and licensing

LAS Recomp by Peiikai and contributors supplies the recompiler and runtime. This port began at `geno55/acnh-recomp` commit `6b45a0c23649786eac1fc39e01ae4a90769c8221`, whose source at that revision is LAS Recomp despite the repository name. Mihawk’s PS5 Vulkan/template work supplies the native graphics and application foundation; the existing Wind Waker PS5 integration supplies platform references.

Project code is MIT; bundled components retain their own licenses. Native binaries also include GPL/LGPL components and must be accompanied by corresponding source and notices. See [NOTICE](NOTICE) and [third-party notices](ps5/THIRD_PARTY_NOTICES.txt).

This is an unofficial fan project, not affiliated with or endorsed by Nintendo or Sony.
