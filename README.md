# Link’s Awakening for PS5

Native PS5 port of the 2019 Switch version of **The Legend of Zelda: Link’s Awakening**, based on [LAS Recomp](https://github.com/peiikai/LASRecomp). Title ID: **PPSA99670**.

The PS5 port uses Mihawk’s Vulkan foundation and the LAS Recomp NVN-to-Vulkan renderer. It runs recompiled game code with a native runtime. You must supply your own **Switch version 1.0.0** game dump and keys. Game files, keys, saves and generated game code are not distributed with this project. Update files are not supported by this build.

[Download v0.1.0](https://github.com/mshivam019/la-ps5/releases/tag/v0.1.0) · [PS5 setup](ps5/README.md) · [Build from source](ps5/BUILD.md)

## Features

- Native homebrew title **Link’s Awakening** (`PPSA99670`), launched from the PS5 home screen.
- 1920×1080 rendering with 3840×2160 FSR1-upscaled output.
- DualSense input, PlayStation button textures, native audio and movie playback.
- Controller-operated launcher and touchpad settings menu.
- Background shader compilation with persistent caches.
- Saves, settings and caches stored alongside the title on M.2.

## Requirements

- A homebrew-capable PS5 with native folder-title support and a compatible registration/mounting setup.
- Your own **Switch version 1.0.0** base-game dump and keys.
- A computer for preparing your game files and enough free M.2 space for the title.

## Installation

1. Install **PPSA99670.zip** using your native-title launcher or PS5 Upload.
2. Extract the matching **Linux setup ZIP** on your computer (Windows users can use WSL2).
3. Run `sh setup.sh --nsp /path/to/base-game.nsp --keys /path/to/prod.keys --output /path/to/prepared/PPSA99670`.
4. Close the game and copy the generated `extracted/` folder into `/mnt/ext1/etaHEN/games/PPSA99670/`.
5. Launch **Link’s Awakening** from the PS5 home screen.

The application and desktop setup are separate downloads. Setup prepares the private game module and assets from your dump. Keep `user/` when updating: it contains saves, settings and shader caches.

See [setup requirements and paths](ps5/README.md), [build instructions](ps5/BUILD.md), [release packaging](ps5/RELEASING.md), and [upstream documentation](README.upstream.md).

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
