# Building for PS5

The PS5 target uses Clang, the native payload SDK, SDL3, Mesa/RADV, shaderc and
FFmpeg. Dependency versions are recorded in `dependencies.json`; releases
include corresponding dependency source and notices.

## Workspace

The native build currently uses the same workspace layout as the other ports:

```text
Links-Awakening-PS5/
PS5_PayloadSDK/
PS5_Vulkan/
Dusklight-PS5/.deps/src/SDL/
Dusklight-PS5/.deps/src/mesa/
Wind-Waker-HD-PS5/build/sdl3-ps5/libSDL3.a
```

Compiler wrappers and the base SDK are read from
`~/ps5sdk/native-app-boilerplate`. The build prepares its own SDK copy under
`.deps/native`. See `cmake/native.cmake` for toolchain overrides.

## Dependencies and game preparation

```sh
python3 ps5/tools/setup-deps.py
python3 ps5/tools/extract-game.py --nsp /private/base-game.nsp --keys /private/prod.keys --output /private/extracted
python3 ps5/tools/generate-game.py --exefs /private/extracted/exefs
```

The generator checks the main executable’s build ID and requires Switch version
1.0.0. Keep the base-game dump, keys, extracted files and generated code private.
Extraction and generation refuse to overwrite existing output.

## Native application

```sh
python3 ps5/tools/build-title.py --dependencies
```

For incremental changes, omit `--dependencies`. Avoid `--clean` unless a complete
rebuild is necessary. Build and packaging commands do not deploy or launch a title.
See [setup](README.md) for game-module preparation and installation.

Mihawk’s Vulkan foundation supplies the RADV link recipe, native CRT/SELF tool,
volk dispatch and platform reference. The game retains the NVN-to-Vulkan renderer.
The native application uses VK_KHR_display, static shaderc compilation, SDL audio,
and a PS5-pad-to-SDL virtual-controller bridge. FFmpeg supplies movie decoding
with x86 assembly enabled. The NASM build helper can prepare a pinned local copy.

## Focused checks

```sh
bash ps5/tools/test-memory.sh
bash ps5/tools/test-filesystem.sh
bash ps5/tools/test-input.sh
bash ps5/tools/test-ipa.sh
bash ps5/tools/test-prewarm-cache.sh
python3 ps5/tools/test-cache-writer.py
```

These checks cover memory mapping, save paths, controller mapping, shader
translation and cache handling. Runtime integration uses retained ELF registries
for HLE bindings, reset callbacks and IPC services.

## Runtime paths

Prepared game data uses `/app0/extracted`. Settings, saves and caches use
`/app0/user`, alongside the title on M.2. `LARECOMP_DATA_ROOT` can override the
default data location. The native platform supplies a virtual working directory
for sandbox-compatible file access. Desktop extraction and helper processes run
on the computer during setup.

When updating an installation, close the title first and preserve `user/`.
