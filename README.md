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
Menu dikelompokkan per kategori, dan setiap kategori adalah section yang bisa dibuka-tutup.
Semuanya tertutup saat menu dibuka, jadi daftar fitur tetap ringkas.

**Belanja**

- Item bisa dibeli tanpa Play Billing dan tanpa biaya, dan semua tile, model, serta melody
  langsung terbuka.
- Semua jenis slot penyimpanan terbuka tanpa batas.

**Nama dan profil**

- Batas nama profil, tanggal lahir, teks perkenalan, dan kotak sandi room dinaikkan.
- Slider posisi skin X/Y dan portrait X/Y melebar dari -5..5 menjadi -25..25.
- Kata terlarang tidak disaring lagi di perangkat ini.
- Label versi selalu diberi akhiran ` (modded)`; tidak ada sakelar untuk mematikannya.

**Room dan dunia**

- Save world tetap bisa dilakukan walaupun host mematikan izinnya di awal.
- Room bersandi bisa dimasuki tanpa mengisi sandi.
- Room dengan ukuran di atas batas perangkat tetap terdaftar dan bisa dimasuki.
- Ukuran dunia baru bisa dipilih sampai 600x300.
- **Daftar pemain** di menu menampilkan host, pemain sendiri, dan tamu yang sedang ada di room,
  masing-masing dengan lencana peran `host`, `kamu`, atau `tamu`. Daftarnya diperbarui dari
  thread game tiap 0,4 detik dan disegarkan ulang di menu tiap 2 detik.
- **Salin skin** menulis model pemain yang dipilih ke slot model kosong pertama, lalu menulis
  nama pemiliknya ke `Name.txt` slot itu. Model dicatat di memori saat tamu masuk room; bila
  tamu itu sudah ada sebelum mod dimuat, modelnya dibaca dari daftar tamu yang sedang hidup.
  Tidak ada skin yang tersimpan tanpa tombol ditekan.
- **Tendang** hanya muncul untuk tamu, dan mengirim paket tendang serta paket penghapusan tamu
  sebagai host, sama seperti tombol tendang bawaan. Hasil setiap aksi dikabarkan lewat toast.

**Akun**

- **Tampilkan detail akun yang disembunyikan** membuat baris privasi akun menampilkan detail
  yang dipilih pemiliknya, bukan menyembunyikannya.

**Chat**

- Pesan pribadi ikut tampil di chat room sebagai `[PM -> nama penerima]` dan dicatat di log.

**Senjata dan item**

- Pistol dan buster menembak berulang selama tombol interaksi ditahan.
- Buster selalu memakai amunisi terkuat.
- Kotoran tetap di tangan, bisa dilempar berulang, dengan kekuatan penuh dan arah sesuai bidikan.
- Gingerbread dan hot cross bun tetap di tangan setelah dimakan.
- Ledakan tidak lagi membuat pemain terpental.

**Performa**

- Getar kamera dan serpihan tile dimatikan.

**Gerak**

- **No Clip** (tembus dinding dan benda) melebarkan kotak gerak pemain ke kiri, kanan, dan atas
  pada tiap pemeriksaan fisika tile, jadi dinding dan langit-langit tidak lagi menahan langkah.
  Sisi bawah dibiarkan apa adanya supaya lantai tetap dipijak.
- **Pindahkan pemain** memindahkan pemain ke titik X/Y yang diisi di menu. Nilainya adalah
  koordinat tile dunia yang dipakai fisika game, sama seperti yang ditulis game saat teleport
  bawaan, bukan piksel layar. Tombol hanya menitipkan permintaan; posisi baru dipasang di hook
  update pemain pada thread game, sekaligus memindahkan seluruh titik jejak fisika, menolkan
  kecepatan, dan meminta tile di sekitar badan dibangun ulang supaya pemain tidak kembali ke
  tempat semula.

## Batasan

- Hanya `arm64-v8a`. Tidak ada komponen Java tambahan, Xposed, atau LSPosed di luar menu LGL.
- Daftar pemain hanya terisi selama berada di dalam room; di luar room section itu kosong.
- Cache model tamu menyimpan 16 tamu terakhir. Tamu yang sudah keluar tidak lagi ditawarkan,
  dan model tamu yang tidak ada di cache maupun di daftar tamu hidup dilaporkan lewat toast
  "belum tercatat", bukan disalin dengan tebakan.
- Teleport tidak memeriksa isi tile di titik tujuan, jadi memindahkan pemain ke dalam dinding bisa
  membuat fisika game mendorongnya keluar lagi.
- Koordinat Teleport adalah tile dunia. Titik yang jauh di luar peta bisa membuat pemain jatuh
  tanpa lantai di bawahnya.

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
Scan selesai: 85 dari 85 metode ditemukan
Field model skin OK
Field daftar tamu OK
Field nama tamu siap
Field daftar pemain siap
Field pemain OK
Field tembus dinding siap
Hook terpasang: 66
```

Bila `Hook terpasang` jauh di bawah 66, baris `Hook gagal:` atau `Target tidak ditemukan:` di
atasnya menyebut method yang tidak cocok dengan build game.

Saat proses mati karena sinyal, log yang sama menyimpan blok `ERROR CRASH`:

```
ERROR CRASH signal=SIGABRT code=-1 pc=0x... module=libc.so+0x... fault=0x... tid=...
ERROR CRASH regs lr=0x... x19=0x... x20=0x... sp=0x...
ERROR CRASH thread=NamaThread tid=...
ERROR CRASH abort message=...
ERROR CRASH abort caller=libil2cpp.so+0x...
ERROR CRASH frame libil2cpp.so+0x...
```

`abort caller` adalah fungsi yang memanggil `abort()`, jadi baris itu biasanya menyebut
pemeriksaan mana yang menyerah lebih dulu. Bila alamatnya tidak jatuh di modul yang terpotret,
barisnya ditulis `raw=0x...` supaya masih bisa dicocokkan dengan dump atau tombstone.
