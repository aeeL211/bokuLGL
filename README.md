# BokuLGL

Mod menu untuk **Boku Boku** (`com.pixticle.bokuboku`), dibangun dari template
[LGL Mod Menu](https://github.com/LGLTeam/Android-Mod-Menu). Seluruh hook ditulis di C++ dan
memakai IL2CPP lewat nama kelas dan method, bukan offset. Hook memakai Dobby yang sudah
disertakan template dan ikut di dalam satu `libboku.so`.

Hanya `arm64-v8a`, Android 7.0 ke atas.

## Pemasangan

`bokuLGL-debug.apk` adalah menu, bukan game. Library hook baru berjalan bila
`lib/arm64-v8a/libboku.so` dimuat di dalam proses **Boku Boku**:

- gabungkan isi APK ini ke APK game (menu, `classes.dex`, dan `lib/arm64-v8a/libboku.so`),
  lalu pasang APK hasil gabungan; atau
- suntikkan `libboku.so` ke proses game yang sudah berjalan.

Memasang `com.android.support` sendirian hanya membuka menu di prosesnya sendiri: game tidak
pernah memuat library itu, jadi tidak ada satu pun hook yang terpasang.

## Yang diubah

Semua fitur menyala sejak awal kecuali **No Clip**. Sakelar di menu hanya mengubah nilai yang
dibaca hook pada tiap panggilan; hook tidak pernah dipasang atau dilepas saat game berjalan.
Menu dikelompokkan per kategori.

**Ekonomi**

- Pembelian IAP disimulasikan tanpa Play Billing, dan semua tile, model, serta melody langsung
  terbuka.
- Semua jenis slot penyimpanan terbuka tanpa batas.

**Input dan profil**

- Batas nama profil, tanggal lahir, teks perkenalan, dan kotak sandi room dinaikkan.
- Slider posisi skin X/Y dan portrait X/Y melebar dari -5..5 menjadi -25..25.
- Kata terlarang tidak disaring lagi di perangkat ini.
- Label versi diberi akhiran ` (modded)`.

**Room dan dunia**

- Save world tetap bisa dilakukan walaupun host mematikan izinnya di awal.
- Room bersandi bisa dimasuki tanpa mengisi sandi.
- Room dengan ukuran di atas batas perangkat tetap terdaftar dan bisa dimasuki.
- Ukuran dunia baru bisa dipilih sampai 600x300.
- `/kick all` di chat mengeluarkan semua tamu satu per satu.
- **Daftar pemain** di menu menampilkan tamu yang sedang ada di room, lengkap dengan tombol
  **Tendang** per pemain. Daftarnya diperbarui dari thread game tiap 0,4 detik dan disegarkan
  ulang di menu tiap 2 detik; host dan pemain sendiri tidak ikut ditampilkan.

**Akun dan privasi**

- Baris privasi akun tidak lagi menyembunyikan detail yang dipilih pemiliknya.
- Skin yang terlihat di room disalin otomatis ke slot model kosong pertama, dan nama pemiliknya
  ditulis ke `Name.txt` slot itu.

**Chat dan sosial**

- Pesan pribadi ikut tampil di chat room sebagai `[PM -> nama penerima]` dan dicatat di log.
- Tombol tendang muncul di panel tamu.

**Aksi pemain**

- Pistol dan buster menembak berulang selama tombol interaksi ditahan.
- Buster selalu memakai amunisi terkuat.
- Kotoran tetap di tangan, bisa dilempar berulang, dengan kekuatan penuh dan arah sesuai bidikan.
- Gingerbread dan hot cross bun tetap di tangan setelah dimakan.
- Ledakan tidak lagi membuat pemain terpental.

**Tampilan**

- Getar kamera dan serpihan tile dimatikan.

**Gerak**

- **No Clip** mematikan `Collider2D` pemain bila ada, atau membuat `Rigidbody2D`-nya
  kinematik bila hanya komponen itu yang ada. Game ini memakai fisika tile sendiri, jadi bila
  pemain tidak membawa komponen Unity tersebut, fitur tidak berefek dan alasannya dicatat di log.
- **Teleport** memindahkan pemain ke koordinat X/Y yang diisi di menu. Tombol hanya menitipkan
  permintaan; posisi baru dipasang di hook update pemain pada thread game.

## Batasan

- Hanya `arm64-v8a`. Tidak ada komponen Java tambahan, Xposed, atau LSPosed di luar menu LGL.
- RoomKit hanya menerima `/kick`; bentuk lain dianggap chat biasa.
- Daftar pemain hanya terisi selama berada di dalam room; di luar room section itu kosong.
- Teleport memakai `Transform.set_position`, jadi bila fisika tile game menulis ulang posisi
  pada frame yang sama, pemain bisa kembali ke tempat semula.

## Build

Build hanya lewat GitHub Actions (`.github/workflows/build.yml`): NDK r30, CMake, lalu
`assembleDebug`. Dobby sudah ada di `app/src/main/jni/Dobby/arm64-v8a/libdobby.a`, jadi tidak ada
unduhan tambahan saat build. Hasilnya `bokuLGL-debug.apk` dengan `lib/arm64-v8a/libboku.so`.

Parameter CMake yang dipakai CI:

- `BOKU_BUILD_SEED`: `github.run_id`, kunci tabel target. Setiap build memakai blob berbeda.
- `BOKU_BUILD_COMMIT`: `github.sha`, ditampilkan di log.

## Log

Setiap sesi menulis `Android/data/<package>/files/bokumods-N.log`. Baris penting saat start:

```
Dobby siap
Scan selesai: 107 dari 107 metode ditemukan
Field pemain OK
Field nama tamu siap
Field daftar pemain siap
Field RoomKit siap
Hook terpasang: 79
```

Bila `Hook terpasang` jauh di bawah 79, baris `Hook gagal:` atau `Target tidak ditemukan:` di
atasnya menyebut method yang tidak cocok dengan build game.
