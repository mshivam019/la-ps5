# Release packaging

Keep the same title ID, **PPSA99670**, for every release. Increase `contentVersion`
in `sce_sys/param.json` for later releases and commit it before tagging.

A release contains the native application ZIP, matching desktop setup/source,
corresponding dependency source and SHA-256 checksums. The application ZIP has
`PPSA99670/` as its top-level folder, with `eboot.bin` and `sce_sys/param.json`.
It must not contain generated game code, extracted game assets, keys, saves or
local configuration. Game preparation produces private files on the player’s
computer, separate from distributable assets.

Use the packaging tools under `ps5/tools/`. Include the license texts under
`ps5/licenses`, `THIRD_PARTY_NOTICES.txt` and artwork attribution. Dependency
source can be archived with:

```sh
python3 ps5/tools/package-sources.py --output /release/Links-Awakening-PS5-dependency-sources.tar.gz
```

The script records source revisions and hashes every source file after packing.
Keep local logs, console screenshots and deployment receipts outside Git.

## Homebrew catalog

Follow the [catalog submission guide](https://github.com/blackbearreloaded/ps5-homebrew-catalog/blob/main/docs/submitting.md).
Publish a versioned release asset named `PPSA99670.zip` and use its SHA-256 in
`apps/PPSA99670.json`. Pin the icon URL to the release tag at
`ps5/sce_sys/icon0.png`. The repository must be public and GitHub must detect its
license before online verification can succeed.

In a catalog checkout containing the record:

```sh
python3 -m catalog check
python3 -m catalog verify PPSA99670
```

Open the PR from the repository owner’s account and change only that record.
Do not replace an asset after submission; publish a new version instead.
