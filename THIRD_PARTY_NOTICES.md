# Third-party notices

`libboku.so` links the following third-party code. Dobby is the only native component shipped
with the repository; everything else comes from the LGL template or the Android NDK.

## Dobby

- Upstream: https://github.com/jmpews/Dobby
- Version: the prebuilt archive bundled with the LGL Mod Menu template, built from upstream
  commit `0932d69c320e786672361ab53825ba8f4245e9d3`.
- License: Apache License 2.0. A copy is kept at `app/src/main/jni/Dobby/LICENSE`.
- How it is used: `app/src/main/jni/Dobby/arm64-v8a/libdobby.a` is linked statically into
  `libboku.so`. Only `DobbyHook` is called, from `boku/hook.cpp`. The archive is linked with
  `--exclude-libs,ALL`, so no `Dobby` or `dobby_` symbol is exported.
- No source changes: the archive is used as shipped by the template.

The full license text is collected at build time into `THIRD_PARTY_LICENSES`, which is published
next to `libboku.so` in the `bokuLGL-apk` artifact.

## Itim

- Upstream: https://github.com/google/fonts/tree/main/ofl/itim
- License: SIL Open Font License 1.1. The license text is kept at
  `app/src/main/assets/fonts/OFL.txt` and packaged as `assets/fonts/OFL.txt`.
- How it is used: `app/src/main/assets/fonts/itim.ttf` is loaded at runtime through
  `Typeface.createFromAsset` in `Theme.applyFont()`. The font file is not modified.

## LGL Mod Menu

- Upstream: https://github.com/LGLTeam/Android-Mod-Menu
- License: GNU General Public License v3.0. The template's `LICENSE` is kept in this repository.
- How it is used: the floating menu, its Java bridge and its JNI registration come from the
  template. KittyMemory and xDL are not used; hooking goes through the bundled Dobby archive.
