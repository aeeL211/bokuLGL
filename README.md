# BokuLGL

Mod menu untuk **Boku Boku** (`com.pixticle.bokuboku`), dibangun dari template
[LGL Mod Menu](https://github.com/LGLTeam/Android-Mod-Menu). Seluruh hook ditulis di C++ dan
memakai IL2CPP lewat nama kelas dan method, bukan offset. ShadowHook ikut di dalam satu
`libboku.so`.

Hanya `arm64-v8a`, Android 7.0 ke atas.

## Yang diubah

Semua fitur menyala sejak awal kecuali **No Clip**. Sakelar di menu hanya mengubah nilai yang
dibaca hook pada tiap panggilan; hook tidak pernah dipasang atau dilepas saat game berjalan.

**Ekonomi dan konten**

- Pembelian IAP disimulasikan tanpa Play Billing, dan semua tile, model, serta melody langsung
  terbuka.
- Semua jenis slot penyimpanan terbuka tanpa batas.

**Input dan profil**

- Batas nama profil, tanggal lahir, teks perkenalan, dan kotak sandi room dinaikkan.
- Slider posisi skin X/Y dan portrait X/Y melebar dari -5..5 menjadi -25..25.
- Kata terlarang tidak disaring lagi di perangkat ini.

**Room dan dunia**

- Save world tetap bisa dilakukan walaupun host mematikan izinnya di awal.
- Room bersandi bisa dimasuki tanpa mengisi sandi.
- Room dengan ukuran di atas batas perangkat tetap terdaftar dan bisa dimasuki.
- Ukuran dunia baru bisa dipilih sampai 600x300.
- Baris privasi akun tidak lagi menyembunyikan detail yang dipilih pemiliknya.

**Pemain lain**

- Skin yang terlihat di room disalin otomatis ke slot model kosong pertama, dan nama pemiliknya
  ditulis ke `Name.txt` slot itu.
- Pesan pribadi ikut tampil di chat room sebagai `[PM -> nama penerima]` dan dicatat di log.
- Tombol tendang muncul di panel tamu, dan `/kick all` di chat mengeluarkan tamu satu per satu.

**Gerak dan aksi**

- Pistol dan buster menembak berulang selama tombol interaksi ditahan.
- Buster selalu memakai amunisi terkuat.
- Kotoran tetap di tangan, bisa dilempar berulang, dengan kekuatan penuh dan arah sesuai bidikan.
- Gingerbread dan hot cross bun tetap di tangan setelah dimakan.
- Getar kamera dan serpihan tile dimatikan.
- Ledakan tidak lagi membuat pemain terpental.
- Label versi diberi akhiran ` (modded)`.

**Fitur baru**

- **No Clip** mematikan `Collider2D` pemain bila ada, atau membuat `Rigidbody2D`-nya
  kinematik bila hanya komponen itu yang ada. Game ini memakai fisika tile sendiri, jadi bila
  pemain tidak membawa komponen Unity tersebut, fitur tidak berefek dan alasannya dicatat di log.
- **Teleport** memindahkan pemain ke koordinat X/Y yang diisi di menu. Tombol hanya menitipkan
  permintaan; posisi baru dipasang di hook update pemain pada thread game.

## Batasan

- Hanya `arm64-v8a`. Tidak ada komponen Java tambahan, Xposed, atau LSPosed di luar menu LGL.
- RoomKit hanya menerima `/kick`; bentuk lain dianggap chat biasa.
- Teleport memakai `Transform.set_position`, jadi bila fisika tile game menulis ulang posisi
  pada frame yang sama, pemain bisa kembali ke tempat semula.
- Nama library harus tetap `libboku.so`. Nama lain membuat tabel target gagal didekripsi dan
  tidak ada hook yang dipasang.

## Build

Build hanya lewat GitHub Actions (`.github/workflows/build.yml`): NDK r30, CMake, lalu
`assembleDebug`. ShadowHook diambil dari upstream pada saat build, tidak disimpan di repo.
Hasilnya `bokuLGL-debug.apk` dengan `lib/arm64-v8a/libboku.so` di dalamnya.

Parameter CMake yang dipakai CI:

- `SHADOWHOOK_ROOT`: hasil ekstrak sumber ShadowHook.
- `BOKU_BUILD_SEED`: `github.run_id`, kunci tabel target.
- `BOKU_BUILD_COMMIT`: `github.sha`, ditampilkan di log.
- `BOKU_LIB_NAME`: `libboku.so`, harus sama dengan nama berkas yang dimuat.

## Log

Setiap sesi menulis `Android/data/<package>/files/bokumods-N.log`. Baris penting saat start:

```
Scan selesai: 107 dari 107 metode ditemukan
Field pemain OK
Hook terpasang: 79
```

Bila muncul baris `Protection:`, build tidak cocok dengan lingkungannya dan tidak ada hook yang
dipasang.
