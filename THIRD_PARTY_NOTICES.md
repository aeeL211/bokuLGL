# Third-party notices

`libboku.so` links the following third-party code. Nothing is vendored in this repository:
every component is fetched from its upstream release at build time.

## ShadowHook

- Upstream: https://github.com/bytedance/android-inline-hook
- Version: v2.0.1, pinned in `.github/workflows/build.yml` (`SHADOWHOOK_TAG`) and overridable
  with the `shadowhook_tag` workflow input.
- License: MIT
- How it is used: the upstream source archive is extracted during the CI build and compiled
  into `libboku.so` as a static archive. `sh_jni.c` is excluded, so the binary contains no
  ShadowHook `JNI_OnLoad`. Public ShadowHook symbols are hidden and are not exported.
- Local modification: one call in `shadowhook.c` is rewritten at build time so that a failed
  `sh_linker_init()` is not fatal. A single-file build ships no `libshadowhook_nothing.so`, and
  upstream uses that stub for the soinfo field-offset scan. The scan serves
  `shadowhook_hook_sym_name` and dlopen tracking, neither of which this mod calls;
  `shadowhook_hook_func_addr`, the only API used here, is unaffected.

## xdl

- Upstream: https://github.com/hexhacking/xdl, bundled inside ShadowHook under
  `shadowhook/src/main/cpp/third_party/xdl`
- License: Apache License 2.0
- How it is used: compiled from the same ShadowHook source archive and linked statically.

## linux-syscall-support

- Upstream: https://chromium.googlesource.com/linux-syscall-support, bundled inside ShadowHook
  under `shadowhook/src/main/cpp/third_party/lss`
- License: Apache License 2.0 (BSD-style option)
- How it is used: headers only, compiled into the ShadowHook objects.

The full license texts are collected at build time into `THIRD_PARTY_LICENSES`, which is
published next to `libboku.so` in the `bokuLGL-apk` artifact.

## LGL Mod Menu

- Upstream: https://github.com/LGLTeam/Android-Mod-Menu
- License: GNU General Public License v3.0. The template's `LICENSE` is kept in this repository.
- How it is used: the floating menu, its Java bridge and its JNI registration come from the
  template. Dobby, KittyMemory and xDL are removed; hooking uses ShadowHook instead.
