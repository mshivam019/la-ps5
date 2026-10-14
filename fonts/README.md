# System fonts

The game draws its text with the Switch's built-in system font, which isn't part of the game files.
Without it, LAS Recomp uses Hiragino Sans (built into macOS, the same style of Japanese gothic as the Switch's
Morisawa UD Shin Go), then Arial.

For the exact look, put a dump from your own console here (Switch system archive *FontStandard*, title
0100000000000811, and *FontNintendoExtension*, 0100000000000810):

- `nintendo_udsg-r_std_003.bfttf`: the main font (a decrypted `.ttf` with the same name, or spelled `nintendo_udsgr_std_003`, also works)
- `nintendo_ext_003.bfttf`: button glyphs (optional)

`LARECOMP_FONT=<file>` overrides everything (.ttf / .otf / .ttc / .bfttf).
