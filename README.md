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

Manifest menu membawa service `Launcher` dan provider `AutoStart`. Keduanya harus ikut
digabung ke manifest APK game. Provider hanya menyalakan ulang menu saat Android membuat ulang
proses game tanpa melewati `MainActivity`, misalnya ketika activity game dipulihkan langsung
sesudah proses dibuang di latar belakang.

## Yang diubah

Semua fitur menyala sejak awal kecuali **No Clip**, **Skin & Portrait Auto Dump**, dan
**Atur dunia sendiri**. Sakelar di menu hanya mengubah nilai yang
dibaca hook pada tiap panggilan; hook tidak pernah dipasang atau dilepas saat game berjalan.
Menu dikelompokkan per kategori, dan setiap kategori adalah section yang bisa dibuka-tutup.
Semuanya tertutup saat menu dibuka, jadi daftar fitur tetap ringkas. Tekan lama pada baris
fitur untuk membaca penjelasan singkatnya.

Menu mengikuti game: ikon disembunyikan saat aplikasi lain dipakai dan muncul lagi saat game
kembali. Saat kembali, menu juga memasang ulang jendela overlay bila sistem melepasnya selama
game di latar belakang, dan proses yang dibuat ulang langsung ke activity game tetap
menyalakan menu lewat provider `AutoStart`.

**Room dan Dunia**

- Room bersandi bisa dimasuki tanpa mengisi sandi.
- Save world tetap bisa dilakukan walaupun host mematikan izinnya di awal.
- Room dengan ukuran di atas batas perangkat tetap terdaftar dan bisa dimasuki.
- Ukuran dunia baru bisa dipilih sampai 600x300.
- **Bangun dan hancurkan di room orang lain** memaksa `Enforce` pada paket tile dan `Across`
  pada paket objek, jadi host menerima pemasangan, penggantian, dan penghapusan tile serta
  penambahan dan pengambilan objek dari tamu. Ledakan TNT ikut bekerja: tamu menghitung
  ledakan di perangkatnya sendiri lalu mengirim permintaan hapus per tile dan objek, bukan
  paket `Explosion_Range` yang ditolak host. Kamu tetap harus sudah diterima sebagai tamu;
  area yang dilindungi dan perubahan pintu yang sudah ada tidak ikut ditembus.
- **Atur dunia sendiri** menimpa latar, waktu, dan cuaca dunia hanya di perangkat ini. Ketiga
  pilihan memanggil setter lokal game (`Background__Bg._Set`, `Background__Period._Set`,
  `Background__Weather._Set`) yang juga dipakai klien saat menerima perubahan dari host, jadi
  tidak lewat pemeriksaan admin dan tidak mengirim paket. Host dan tamu lain tetap memakai
  dunia masing-masing; bila host sendiri yang memakai fitur ini, perubahan tetap disiarkan
  seperti perubahan host biasa. Pilihan **Bawaan** membiarkan bagian itu mengikuti host, dan
  saat sakelar dimatikan setiap bagian yang diubah dikembalikan ke pilihan sebelum fitur
  dipakai. Nilai asli dicatat per zona; saat zona berganti, nilai asli diambil ulang dari zona
  yang sedang dimuat. Perubahan host yang masuk sesudahnya ditimpa lagi setelah jeda sekitar
  dua detik, jadi animasi tidak direstart tiap frame.
- **Daftar pemain** di menu menampilkan host, pemain sendiri, dan tamu yang sedang ada di room,
  masing-masing dengan lencana peran `host`, `kamu`, atau `tamu`. Menu membacanya sekali per
  detik selama barisnya terlihat; salinan di sisi native hanya disusun selama dua detik sesudah
  pembacaan terakhir, jadi daftar berhenti disegarkan saat menu ditutup atau section dilipat.
  Kartu yang isinya tidak berubah dipakai ulang, dan pilihan pemain disimpan sebagai kunci
  sehingga tetap menunjuk orang yang sama saat urutan daftar berubah.
- **Copy avatar** menulis potret pemain yang dipilih ke slot potret kosong pertama, memakai
  penulis potret bawaan game. Potret dicatat dari paket akun saat tamu masuk room; bila tamu
  itu sudah ada sebelum mod dimuat, potretnya dibaca dari data akun tamu yang sedang hidup.
  Seperti copy skin, tidak ada avatar yang tersimpan tanpa tombol ditekan.
- **Copy skin** menulis model pemain yang dipilih ke slot model kosong pertama, lalu menulis
  nama pemiliknya ke `Name.txt` slot itu. Model dicatat di memori saat tamu masuk room; bila
  tamu itu sudah ada sebelum mod dimuat, modelnya dibaca dari daftar tamu yang sedang hidup.
  Tidak ada skin yang tersimpan tanpa tombol ditekan.
- **Kick** hanya muncul untuk tamu. Ketuk tombolnya untuk tendangan sementara: hanya paket
  tendang yang dikirim, jadi tamu menerima pemberitahuan tanpa dihapus dari daftar. Tahan
  tombolnya untuk tendangan permanen: paket tendang dan paket penghapusan tamu dikirim sebagai
  host, sama seperti tombol tendang bawaan. Hasil setiap aksi dikabarkan lewat toast.
- **Kick host** muncul di kartu host. Tombol ini mengirim paket tendang dan penghapusan tamu
  berulang ke kunci host, sebagai host. Host bisa lag lalu keluar dari room, dan room akan
  ditutup. Jangan dipakai di room yang masih ingin dipertahankan.
- **Kick semua** ada di header daftar pemain. Satu tekan menendang setiap tamu secara permanen;
  host dan pemain sendiri tidak ikut. Konfirmasi ditampilkan sebelum paket dikirim.
- Pesan pribadi ikut tampil di chat room sebagai `[PM -> nama penerima]` dan dicatat di log.

**Nama dan Profil**

- Batas nama profil, tanggal lahir, dan kotak sandi room dinaikkan. Teks perkenalan tetap
  memakai batas bawaan game.
- **Jarak geser skin** 5 sampai 100 dengan bawaan 25, berlaku untuk slider posisi X/Y model dan
  potret. Nilai 25 berarti -25..+25 seperti sebelumnya, 5 berarti rentang bawaan game -5..+5,
  dan 100 adalah geseran terjauh. Nilai baru dipakai saat editor skin atau potret dibuka lagi.
- **Skin & Portrait Auto Dump** mati secara bawaan. Saat dinyalakan, menu menampilkan
  peringatan lebih dulu karena setiap tamu yang masuk berarti satu berkas skin dan satu berkas
  potret baru. Skin ditulis ke slot model kosong pertama beserta `Name.txt` pemiliknya, potret
  ditulis ke slot potret kosong pertama. Keduanya memakai penulis bawaan game, dan potret yang
  sudah pernah tersimpan tidak ditulis dua kali.
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
  `speed.add.x` pemain lokal tepat sebelum `Physics__Manager.Pos__Check` menghitung langkah
  berikutnya, lalu nilai aslinya dikembalikan. Karena yang dikali hanya tambahan langkah,
  pengali tidak menumpuk dan badan fisika lain tidak ikut tersentuh. Hasil skala dibatasi ke
  `±Physics__Manager._speed.limit`, batas yang sama dengan `Speed_Limit` game dan bernilai
  `_Tile.pixel_Size - 1` (15 satuan). Langkah normal hanya beberapa satuan per frame, jadi di
  atas sekitar 2,6x hasilnya sudah menyentuh batas itu: slider tetap bisa dinaikkan sampai 5,0x,
  tetapi jarak per frame tidak bertambah lagi. Batas ini wajib, karena langkah yang melewati satu
  tile membuat tile di antaranya tidak ikut diperiksa dan render mulai rusak.
- **Gravitasi pemain** 0,0x sampai 3,0x dengan langkah 0,1x dan bawaan 1,0x. Semua gerakan pemain
  lewat `Physics__Manager.Pos__Check`, jadi yang diskalakan adalah komponen turun `speed.add.y`
  tepat sebelum langkah dihitung, lalu nilai aslinya dikembalikan. Ini berlaku juga saat terbang,
  berenang, memanjat turun, dan meluncur, bukan hanya saat jatuh biasa. Komponen turun memakai
  batas `±Physics__Manager._speed.limit` yang sama seperti kecepatan, jadi jatuh yang dipercepat
  tidak pernah melompati satu tile. Nilai 0,0x membuang gerak turun sehingga pemain melayang di
  ketinggian sekarang; dorongan naik dari lompatan tetap diberikan, jadi pemain bisa terus naik
  sampai gravitasi dinaikkan lagi.
- Ketuk angka di kanan slider **Kecepatan pemain** atau **Gravitasi pemain** untuk
  mengembalikan slider ke 1,0x sekaligus mengirim nilai bawaannya ke hook.
- **Pindahkan pemain** memindahkan pemain ke titik X/Y yang diisi di menu. X adalah koordinat tile
  dari tepi kiri dunia. Y memakai satuan UI game: 0 adalah tengah dunia, sama dengan
  `Tile__Manager.Display__Y` yang menghitung `Y tile - h/2` dengan `h` sebagai tinggi dunia yang
  sedang dipakai. Tinggi dunia dibaca dari `Manager.tile._world_Size.h`, lalu Y tile dihitung
  `Y UI + h/2`. Skala diambil dari
  `_Tile.pixel_Size`, yaitu ukuran satu tile dalam satuan dunia, dan titik acuannya adalah posisi
  pemain sekarang: `dunia tujuan = dunia sekarang + (tile tujuan - tile sekarang) * skala`.
  Bila `pixel_Size` tidak terbaca, rasio collider dipakai sebagai cadangan, lalu sampel gerakan
  pemain setelah perpindahan minimal empat tile.
  Teleport menulis `Physics.pos.tile` dan `Physics.pos.world` sekaligus, memindahkan seluruh
  titik jejak `_world`, menolkan kecepatan, lalu meminta tile di sekitar badan dibangun ulang.
  Sebelum, target, sesudah, dan selisih setiap teleport dicatat di log.
- Baris **Posisi pemain** menampilkan tile dan koordinat dunia pemain sekali per detik selama
  barisnya terlihat. Tile Y sudah dalam satuan UI, jadi angkanya bisa langsung disalin ke titik
  teleport.

## Batasan

- Hanya `arm64-v8a`. Tidak ada komponen Java tambahan, Xposed, atau LSPosed di luar menu LGL.
- Pemantau `dlopen` ShadowHook tidak aktif. Mod ini tidak memaketkan pendamping
  `libshadowhook_nothing.so`, dan `sh_linker.c` sengaja tidak menggagalkan init karenanya.
  Semua target hook sudah berada di `libil2cpp.so` yang dimuat sebelum hook dipasang dan tidak
  pernah dilepas, jadi hook, trampolin, dan pemasangan tetap berjalan normal. Yang tidak ada
  hanya hook pada pustaka yang dimuat belakangan dan pelepasan hook otomatis saat `dlclose`.
- Daftar pemain hanya terisi selama berada di dalam room; di luar room section itu kosong.
- **Kick host** bergantung pada cara host memproses paket tendang untuk kunci dirinya sendiri.
  Bila host menolak atau mengabaikan paket berulang, tombol hanya menambah lalu lintas paket dan
  host tidak keluar. Burst juga bisa membuat perangkat pengirim tersendat sebentar.
- **Kick semua** bekerja pada daftar tamu saat aksi dijalankan; tamu yang masuk sesudahnya tidak
  ikut terkena sampai tombol ditekan lagi.
- **Bangun dan hancurkan di room orang lain** hanya melewati izin build/destroy per-tamu.
  Kamu tetap harus sudah diterima host, tile di area yang dilindungi tetap tidak bisa dihapus,
  dan perubahan pintu yang sudah ada lewat `Tile__Modify__Door` tetap ditolak karena jalur itu
  tidak membawa `Enforce`/`Across`. Ledakan TNT dihitung dari salinan dunia di perangkat tamu,
  jadi jangkauannya mengikuti tile yang terlihat di sana, dan pantulan pemain hanya terjadi di
  perangkat yang menghitung ledakan.
- Kecepatan, gravitasi, teleport, dan No Clip hanya berlaku untuk pemain lokal. Fitur yang
  berjalan pada state yang sama untuk semua pemain di dalam room — tembak berulang, amunisi
  buster penuh, kotoran, makanan tetap di tangan, dan tidak terpental — juga hanya menyentuh
  pemain lokal; badan tamu memakai perilaku bawaan game. Nilai asli game tidak ditimpa permanen:
  pengali dipasang selama satu panggilan method game lalu nilainya dikembalikan.
- Gravitasi bekerja dengan menskalakan seluruh komponen turun `speed.add.y`, jadi gerakan turun
  yang disengaja seperti menyelam, meluncur, atau memanjat turun ikut melambat. Nilai 0,0x
  membuang semua gerak turun; dorongan naik dari lompatan tetap dipertahankan. Naikkan gravitasi
  lagi untuk turun.
- Cache model tamu menyimpan 16 tamu terakhir. Tamu yang sudah keluar tidak lagi ditawarkan,
  dan model tamu yang tidak ada di cache maupun di daftar tamu hidup dilaporkan lewat toast
  "belum tercatat", bukan disalin dengan tebakan.
- Teleport tidak memeriksa isi tile di titik tujuan, jadi memindahkan pemain ke dalam dinding bisa
  membuat fisika game mendorongnya keluar lagi. Input di luar -1024..1024 ditolak dan dicatat di
  log.
- Koordinat Teleport memakai X tile dari tepi kiri dan Y satuan UI game (0 adalah tengah dunia).
  Bila field tinggi dunia tidak terbaca, Y diperlakukan sebagai tile apa adanya dan peringatan
  muncul di log. Titik yang jauh di luar peta bisa membuat pemain jatuh tanpa lantai di bawahnya.
- Kalibrasi skala tile memakai `_Tile.pixel_Size` yang pasti. Bila field itu hilang, rasio
  collider atau perpindahan minimal empat tile dipakai sebagai cadangan, dan sebelum ada sampel
  titik tujuan bisa meleset.

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
ShadowHook ready (shadowhook version 2.0.1, multi mode)
dlopen monitor not used
Scan complete: 99 of 99 methods found
Skin model fields OK
Portrait data field OK
Guest list fields OK
Live portrait fields OK
Guest name field ready
Player list field ready
Player field OK
World height field ready
World override fields ready
Tile size field ready
Speed limit field ready
Wall clip field ready
Hooks installed: 79
```

Bila `Hooks installed` jauh di bawah 79, baris `Hook failed:` atau `Target not found:` di
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
