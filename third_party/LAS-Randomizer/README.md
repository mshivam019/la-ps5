# LAS-Randomizer (vendored, trimmed)

The randomizer core of [LAS-Randomizer](https://github.com/Owen-Splat/LAS-Randomizer) by Owen_Splat, Glan and
contributors (MIT, see `LICENSE.txt`), used by LAS Recomp's seed generator (`tools/rando/las_headless.py`).

Only what the generator uses is kept: `RandomizerCore/` (with local fixes, marked "LAS Recomp" / "LARecomp fix")
and `Resources/` (chest textures, the title-screen logo). The desktop app (Qt UI), its build scripts and UI text
resources were removed; see the upstream repository for those.

Python environment: `python3.12 -m venv venv && venv/bin/pip install -r requirements.txt` (the Qt UI and packaging
dependencies are dropped; oead 1.3.0 because the upstream pin has no Python 3.12 build).
