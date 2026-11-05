# BokuLGL

Mod menu untuk **Boku Boku** (`com.pixticle.bokuboku`), dibangun dari template
[LGL Mod Menu](https://github.com/LGLTeam/Android-Mod-Menu). Seluruh hook ditulis di C++ dan
memakai IL2CPP lewat nama kelas dan method, bukan offset. Hook memakai ShadowHook v2.0.1 yang
sumbernya ikut di dalam repo dan dikompilasi menjadi satu `libboku.so`.

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

**Room dan Dunia**

- Room bersandi bisa dimasuki tanpa mengisi sandi.
- Save world tetap bisa dilakukan walaupun host mematikan izinnya di awal.
- Room dengan ukuran di atas batas perangkat tetap terdaftar dan bisa dimasuki.
- Ukuran dunia baru bisa dipilih sampai 600x300.
- **Daftar pemain** di menu menampilkan host, pemain sendiri, dan tamu yang sedang ada di room,
  masing-masing dengan lencana peran `host`, `kamu`, atau `tamu`. Menu membacanya sekali per
  detik selama barisnya terlihat; salinan di sisi native hanya disusun selama dua detik sesudah
  pembacaan terakhir, jadi daftar berhenti disegarkan saat menu ditutup atau section dilipat.
  Kartu yang isinya tidak berubah dipakai ulang, dan pilihan pemain disimpan sebagai kunci
  sehingga tetap menunjuk orang yang sama saat urutan daftar berubah.
- **Salin skin** menulis model pemain yang dipilih ke slot model kosong pertama, lalu menulis
  nama pemiliknya ke `Name.txt` slot itu. Model dicatat di memori saat tamu masuk room; bila
  tamu itu sudah ada sebelum mod dimuat, modelnya dibaca dari daftar tamu yang sedang hidup.
  Tidak ada skin yang tersimpan tanpa tombol ditekan.
- **Tendang** hanya muncul untuk tamu, dan mengirim paket tendang serta paket penghapusan tamu
  sebagai host, sama seperti tombol tendang bawaan. Hasil setiap aksi dikabarkan lewat toast.
- Pesan pribadi ikut tampil di chat room sebagai `[PM -> nama penerima]` dan dicatat di log.

**Nama dan Profil**

- Batas nama profil, tanggal lahir, teks perkenalan, dan kotak sandi room dinaikkan.
- **Jarak geser skin** 5 sampai 100 dengan bawaan 25, berlaku untuk slider posisi X/Y model dan
  potret. Nilai 25 berarti -25..+25 seperti sebelumnya, 5 berarti rentang bawaan game -5..+5,
  dan 100 adalah geseran terjauh. Nilai baru dipakai saat editor skin atau potret dibuka lagi.
- Kata terlarang tidak disaring lagi di perangkat ini.
- Label versi selalu diberi akhiran ` (modded)`; tidak ada sakelar untuk mematikannya.
- **Tampilkan detail akun yang disembunyikan** membuat baris privasi akun menampilkan detail
  yang dipilih pemiliknya, bukan menyembunyikannya.

**Belanja**

- Item bisa dibeli tanpa Play Billing dan tanpa biaya, dan semua tile, model, serta melody
  langsung terbuka.
- Semua jenis slot penyimpanan terbuka tanpa batas.

**Senjata dan Item**

- Pistol dan buster menembak berulang selama tombol interaksi ditahan.
- Buster selalu memakai amunisi terkuat.
- Kotoran tetap di tangan, bisa dilempar berulang, dengan kekuatan penuh dan arah sesuai bidikan.
- Gingerbread dan hot cross bun tetap di tangan setelah dimakan.
- Ledakan tidak lagi membuat pemain terpental.

**Performa**

- **Getar kamera dimatikan** membuang dua titik masuk getar, jadi layar tidak bergetar saat
  ledakan atau tembakan.
- **Pecahan tile dimatikan** membuang pemunculan puing saat tile pecah; tile dan rekamannya tetap
  utuh. Kedua sakelar berdiri sendiri, jadi getar bisa dimatikan tanpa menghilangkan pecahan.

**Gerak**

- **No Clip** (tembus dinding dan benda) melebarkan kotak gerak pemain ke kiri, kanan, dan atas
  pada tiap pemeriksaan fisika tile, jadi dinding dan langit-langit tidak lagi menahan langkah.
  Sisi bawah dibiarkan apa adanya supaya lantai tetap dipijak.
- **Kecepatan pemain** 0,5x sampai 5,0x dengan langkah 0,1x dan bawaan 1,0x. Nilai ini mengalikan
  `speed.x` pemain lokal pada tiap frame. Nilai yang pernah ditulis dibagi lagi sebelum dikali,
  jadi pengali tidak menumpuk saat game menulis ulang kecepatannya sendiri.
- **Gravitasi pemain** 0,0x sampai 3,0x dengan langkah 0,1x dan bawaan 1,0x. Game memakai fisika
  tile sendiri, bukan `Rigidbody2D`, jadi yang diskalakan adalah tambahan turun `speed.y` tiap
  frame; lompatan dan pendaratan tidak disentuh.
- Ketuk angka di kanan slider **Kecepatan pemain** atau **Gravitasi pemain** untuk
  mengembalikan slider ke 1,0x sekaligus mengirim nilai bawaannya ke hook.
- **Pindahkan pemain** memindahkan pemain ke titik X/Y yang diisi di menu. Nilainya adalah
  koordinat tile dunia yang dipakai fisika game, bukan piksel layar. Rumus konversinya
  `dunia = offset + tile * skala`, dengan skala dan offset dikalibrasi dari perpindahan pemain
  sendiri; sumbu yang belum punya sampel meminjam skala sumbu lain, dan bila keduanya belum
  terkalibrasi skalanya dianggap 1.
  Teleport menulis `Physics.pos.tile` dan `Physics.pos.world` sekaligus, memindahkan seluruh
  titik jejak `_world`, menolkan kecepatan, lalu meminta tile di sekitar badan dibangun ulang.
  Sebelum, target, sesudah, dan selisih setiap teleport dicatat di log.
- Baris **Posisi pemain** menampilkan tile dan koordinat dunia pemain sekali per detik selama
  barisnya terlihat.

## Batasan

- Hanya `arm64-v8a`. Tidak ada komponen Java tambahan, Xposed, atau LSPosed di luar menu LGL.
- Pemantau `dlopen` ShadowHook tidak aktif. Mod ini tidak memaketkan pendamping
  `libshadowhook_nothing.so`, dan `sh_linker.c` sengaja tidak menggagalkan init karenanya.
  Semua target hook sudah berada di `libil2cpp.so` yang dimuat sebelum hook dipasang dan tidak
  pernah dilepas, jadi hook, trampolin, dan pemasangan tetap berjalan normal. Yang tidak ada
  hanya hook pada pustaka yang dimuat belakangan dan pelepasan hook otomatis saat `dlclose`.
- Daftar pemain hanya terisi selama berada di dalam room; di luar room section itu kosong.
- Kecepatan dan gravitasi hanya berlaku untuk pemain lokal. Nilai asli game tidak disimpan
  permanen: pada 1,0x hook berhenti mengalikan, dan setelah respawn atau pindah scene hook membaca
  nilai baru dari game.
- Cache model tamu menyimpan 16 tamu terakhir. Tamu yang sudah keluar tidak lagi ditawarkan,
  dan model tamu yang tidak ada di cache maupun di daftar tamu hidup dilaporkan lewat toast
  "belum tercatat", bukan disalin dengan tebakan.
- Teleport tidak memeriksa isi tile di titik tujuan, jadi memindahkan pemain ke dalam dinding bisa
  membuat fisika game mendorongnya keluar lagi. Input di luar -1024..1024 ditolak dan dicatat di
  log.
- Koordinat Teleport adalah tile dunia. Titik yang jauh di luar peta bisa membuat pemain jatuh
  tanpa lantai di bawahnya.
- Kalibrasi skala tile per sumbu butuh perpindahan minimal satu tile. Sebelum itu tebakan awal
  dari rasio dunia/tile dipakai bila pemain berada di atas tile 1; kalau tidak, sumbu itu
  meminjam skala sumbu lain atau memakai skala 1 sehingga titik tujuan bisa meleset.

## Build

Build hanya lewat GitHub Actions (`.github/workflows/build.yml`): NDK r30, CMake, lalu
`assembleDebug`. Sumber ShadowHook v2.0.1 sudah ada di `app/src/main/jni/ShadowHook/` dan
dikompilasi menjadi arsip statis di dalam `libboku.so`, jadi tidak ada unduhan tambahan saat
build dan tidak ada `libshadowhook.so` maupun `libshadowhook_nothing.so` terpisah. Hasilnya
`bokuLGL-debug.apk` dengan `lib/arm64-v8a/libboku.so`.

Parameter CMake yang dipakai CI:

- `BOKU_BUILD_SEED`: `github.run_id`, kunci tabel target. Setiap build memakai blob berbeda.
- `BOKU_BUILD_COMMIT`: `github.sha`, ditampilkan di log.

## Log

Setiap sesi menulis `Android/data/<package>/files/bokumods-N.log`. Baris penting saat start:

```
ShadowHook siap (shadowhook version 2.0.1, mode multi)
Pemantau dlopen tidak dipakai
Scan selesai: 86 dari 86 metode ditemukan
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
