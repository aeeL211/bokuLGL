# Third-party notices

`libboku.so` links the following third-party code. ShadowHook is the only native hook
component shipped with the repository; everything else comes from the LGL template or the
Android NDK.

## ShadowHook

- Upstream: https://github.com/bytedance/android-inline-hook
- Version: v2.0.1, commit `854c775c2c3676e57a0f383597ebf420b5204161`.
- License: MIT. A copy is kept at `app/src/main/jni/ShadowHook/LICENSE`.
- How it is used: the upstream C and assembly sources under `app/src/main/jni/ShadowHook/`
  are compiled into a static archive and linked into `libboku.so`. `boku/hook.cpp` calls
  `shadowhook_init`, `shadowhook_hook_func_addr_2`, `shadowhook_get_errno`,
  `shadowhook_to_errmsg`, and `shadowhook_get_version`.
  `sh_jni.c` is not compiled because the menu already provides the only `JNI_OnLoad`;
  `nothing/` is not built because no `libshadowhook.so` is shipped. The archive is linked
  with `--exclude-libs,ALL`, so no `shadowhook_` or `sh_` symbol is exported.
- Bundled third-party code inside ShadowHook:
  - xDL, MIT License, `app/src/main/jni/ShadowHook/third_party/xdl/LICENSE`.
  - linux-syscall-support, BSD 3-Clause License,
    `app/src/main/jni/ShadowHook/third_party/lss/LICENSE`.
  - `queue.h`, BSD 3-Clause License, copyright (c) 1991, 1993 The Regents of the
    University of California.
  - `tree.h`, BSD 2-Clause License, copyright (c) 2002 Niels Provos.
  - The BSD texts are kept at `app/src/main/jni/ShadowHook/third_party/bsd/LICENSE`.

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
  template. KittyMemory is not used; hooking goes through the vendored ShadowHook sources.
