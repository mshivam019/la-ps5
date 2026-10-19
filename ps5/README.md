CURRENT INSTALLATION GUIDE: https://github.com/mshivam019/la-ps5/blob/main/INSTALL.md
Use that guide for internal/USB storage, offline copying or Wi-Fi FTP.
M.2, Ethernet and PS5 Upload are optional. Paths in older instructions below
describe the developer setup, not required hardware or transfer software.

# Installation and controls

## Requirements

- A homebrew-capable PS5 with native folder-title support.
- Your own Switch **Link’s Awakening version 1.0.0 base-game NSP** and `prod.keys`.
- Linux x86-64, Python 3.11+, Clang/LLVM (`clang`, `ld.lld`, `llvm-ar`, `llvm-nm`),
  and 35 GB of free space for preparation. Windows users can use WSL2 Ubuntu.

For Ubuntu 24.04 / WSL2 Ubuntu 24.04, install the host tools with:

```sh
sudo apt install python3 python3-venv clang lld llvm
```

## Install

1. Download and install `PPSA99670.zip` with your native-title launcher or PS5 Upload.
2. Extract the matching `Links-Awakening-PS5-v0.1.0-setup-linux.zip` on your computer.
3. From the extracted folder, run:

```sh
sh setup.sh --nsp /path/to/base-game.nsp --keys /path/to/prod.keys --output /path/to/prepared/PPSA99670
```

4. Setup extracts your dump, recompiles its game code and creates `extracted/`.
5. Close the title. Copy that `extracted/` folder into
   `/mnt/ext1/etaHEN/games/PPSA99670/`, next to `eboot.bin`.
6. Launch **Link’s Awakening** from the PS5 home screen and select Play.

The resulting layout is:

```text
PPSA99670/
  eboot.bin
  sce_sys/
  sce_module/
  extracted/
    game.la
    exefs/
    romfs/
    romfs.bin
  user/          created by the app for saves, settings and caches
```

Use the base game: the generator checks version 1.0.0 and does not accept update
NSPs. Setup creates private game code from your dump; do not redistribute its
output. Keys are read locally and are not uploaded by setup. Keep a matching
app and setup version together. A missing or incompatible module shows setup
instructions in the launcher.

If preparation is interrupted, preserve your dump and keys. Move
`build/private-extracted` aside before rerunning setup; cached generated code
and compiled objects are reused when their inputs match.

## Controls

| DualSense | Action |
| --- | --- |
| Cross | A / confirm |
| Circle | B / back |
| Triangle | X |
| Square | Y |
| Options | Plus / game menu |
| L3 + R3 | Minus |
| Touchpad | Runtime settings menu |

PlayStation prompts are enabled by default. Settings, saves and shader caches
are stored in `user/`. Preserve that folder when updating. Close the game before
replacing installed files. FTP copying alone does not register a new title.
