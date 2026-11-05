# ShadowHook (vendored)

Sumber [ByteDance android-inline-hook](https://github.com/bytedance/android-inline-hook) versi
**v2.0.1**, commit `854c775c2c3676e57a0f383597ebf420b5204161`. Lisensi MIT, salinan teksnya ada
di `LICENSE`.

Hanya isi `shadowhook/src/main/cpp` yang disalin ke sini, dan hanya `arch/arm64` yang dipakai
karena mod ini hanya mendukung `arm64-v8a`. Berkas berikut sengaja tidak dibangun:

- `sh_jni.c`: mendefinisikan `JNI_OnLoad` kedua dan Java API. Menu sudah punya `JNI_OnLoad`
  sendiri, dan mod ini tidak memakai API Java ShadowHook.
- `nothing/`: pustaka `.so` kosong yang dipakai `sh_linker.c` sebagai pemicu pemindaian
  `soinfo` untuk pemantau `dlopen`. Mod ini tidak memaketkannya, dan pemantau itu dilewati
  oleh patch lokal di bawah, jadi tidak diperlukan.
- `CMakeLists.txt` dan `shadowhook.map.txt`: build memakai `app/src/main/jni/CMakeLists.txt`
  yang mengompilasi sumber ini menjadi arsip statis di dalam `libboku.so`.

## Patch lokal

Satu perubahan disengaja pada sumber upstream, ditandai `LIBBOKU PATCH` di
`sh_linker.c`. Fungsi `sh_linker_init()` mengembalikan 0 walau simbol privat linker atau
`libshadowhook_nothing.so` tidak tersedia, sehingga `shadowhook_init()` tidak gagal dengan
`SHADOWHOOK_ERRNO_INIT_LINKER` (12). Tanpa patch ini init berhenti dan tidak ada hook yang
terpasang di perangkat yang linker-nya tidak bisa dipindai.

Yang hilang tanpa pemantau `dlopen`: hook pada pustaka yang dimuat belakangan lewat
`shadowhook_hook_sym_name()` dan pelepasan hook otomatis saat pustaka di-`dlclose()`. Mod ini
tidak memakai keduanya; semua target sudah ada di `libil2cpp.so` yang tidak pernah dilepas.
Saat memperbarui versi ShadowHook, patch ini harus diterapkan ulang dan `hook.cpp` disesuaikan.

Kode pihak ketiga di dalam sumber ini:

- `third_party/xdl` - MIT, lihat `third_party/xdl/LICENSE`.
- `third_party/lss` - BSD 3-Clause, lihat `third_party/lss/LICENSE`.
- `third_party/bsd/queue.h` - BSD 3-Clause; `third_party/bsd/tree.h` - BSD 2-Clause, lihat
  `third_party/bsd/LICENSE`.

Jangan menyunting berkas di folder ini. Untuk memperbarui, ganti seluruh isi folder dari tag
upstream yang baru, lalu sesuaikan `ShadowHook/README.md` dan `THIRD_PARTY_NOTICES.md`.
