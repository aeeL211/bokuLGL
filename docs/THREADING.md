# Threading dan kepemilikan state

Dokumen ini mencatat siapa yang menulis dan membaca state bersama setelah
refaktor. Tujuannya bukan menggantikan TSan, tetapi membuat setiap akses bisa
diperiksa ulang saat hook baru ditambahkan.

## Definisi thread

| Nama | Asal | Pekerjaan |
|---|---|---|
| Thread UI | Thread utama Android | Memanggil `GetFeatureList`, `SettingsList`, `Changes`, `DaftarPemain`, `PosisiPemain`, `TendangPemain`, dan `SalinSkinPemain` lewat JNI |
| Thread game | Thread Unity/IL2CPP | Menjalankan proxy hook dan membaca state fitur |
| Thread pekerja | Dibuat `saatMuat()` di `boku/main.cpp` | Menunggu activity, memindai target, memasang hook, lalu berhenti |
| Thread toast | Dibuat `tampilkanToast()` bila thread pemanggil tidak punya Looper | Menyiapkan Looper, menampilkan satu toast, lalu menjalankan `Looper.loop()` |
| Thread sinyal | Thread mana pun yang menerima SIGSEGV/SIGBUS/SIGILL/SIGABRT | Menulis blok crash langsung ke `fdLog` |

Thread pekerja berhenti setelah `toastMasuk()`; tidak ada thread mod yang
memasang atau melepas hook selama permainan berjalan.

## State bersama

| State | Penulis | Pembaca | Penjaga |
|---|---|---|---|
| `Fitur` di `boku/fitur.h` | Thread UI lewat `Changes` | Thread game | Setiap field `std::atomic` |
| `PermintaanTeleport` | Thread UI | Thread game | Tiga field `std::atomic` |
| `pemain::daftarPemain`, `jumlahPemain` | Thread game (`segarkanDaftar`) | Thread UI (`salinDaftar`) | `std::mutex kunciDaftar` |
| `pemain::antrean`, `jumlahAntrean` | Thread UI (`antrekan*`) | Thread game (`ambilAksi`) | `std::mutex kunciAksi` |
| `pemain::posisiRingkas` | Thread game (`simpanPosisi`) | Thread UI (`salinPosisi`) | `std::mutex kunciPosisi` |
| `pemain::daftarDiminta`, `posisiDiminta` | Thread UI | Thread game | `std::atomic<double>` |
| `pemain::layananSiap` | Thread pekerja | Thread UI | `std::atomic<bool>` |
| `mainLibLoaded` | Thread pekerja | Thread UI (`IsGameLibLoaded`) | `std::atomic<bool>` |
| `vmTersimpan`, `kelasUnity` | `JNI_OnLoad` / thread pekerja | Semua jalur JNI | `std::atomic<JavaVM*>` dan `std::atomic<jclass>` |
| Cache kelas/metode JNI | Pemanggil pertama | Semua jalur JNI | `std::mutex kunciCache`; flag `std::atomic<bool>` per entri |
| `jumlahTerpasang`, `hookSiap`, `hookGagal` | Thread pekerja | Thread pekerja dan pembaca jumlah | `std::atomic`; `inisialisasiHook` memakai `std::mutex kunciInit` |
| `fdLog`, ring `tertunda`, `jumlahTertunda` | Semua thread | Semua thread | `fdLog` atomik; ring dilindungi `std::atomic_flag flagTertunda` |
| `pathSesi` | `bukaFileLog` | `pathFileLog` (saat ini tidak dipakai) | Ditulis sekali di bawah `kunciBuka` |
| Tabel `Target`, `Field`, dan cache nama target | Thread pekerja saat scan | Thread game | Tidak ditulis setelah scan; dianggap immutable |

`Target::terpasang` dan `Target::stub` hanya ditulis thread pekerja saat
pemasangan. `Target::orig` diisi ShadowHook sebelum proxy bisa dipanggil; setiap
proxy tetap memeriksa `orig` sebelum memanggilnya.

## Urutan start dan happens-before

1. `saatMuat()` membuat thread pekerja. Thread ini belum menyentuh IL2CPP.
2. `JNI_OnLoad` menyimpan `JavaVM`, kelas `UnityPlayer`, membuka log, lalu
   mendaftarkan native dan cache JNI.
3. Thread pekerja menunggu `kelasUnityPlayer()`, memindai `/proc/self/maps`,
   memanggil `ikatApi()`, memasang penangan crash, menginisialisasi ShadowHook,
   lalu memindai target.
4. Setelah scan selesai, thread pekerja memasang hook satu per satu. Sebelum
   langkah ini tidak ada proxy yang bisa dipanggil.
5. Thread UI baru bisa memanggil method menu setelah `JNI_OnLoad` selesai.
   Nilai awal `Fitur` adalah nilai `true`/`false` yang tertulis di struct,
   sehingga menu yang dibuka lebih awal tetap melihat keadaan yang benar.

## Aturan JNI

- `JNIEnv` tidak pernah disimpan. `EnvScope` memanggil `lekatJni`, dan
  destruktornya hanya memanggil `DetachCurrentThread` bila thread itu yang
  dipasang.
- Setiap referensi lokal memakai `LocalRef`, kecuali beberapa jalur lama di
  `direktoriFile()` dan `akarPenyimpananEksternal()` yang menghapus referensi
  secara manual dan tetap berpasangan di semua jalur keluar.
- `bersihkanEksepsi()` dipanggil setelah pemanggilan Java yang bisa gagal.
  Kegagalan tidak pernah dibiarkan menyeberang ke pemanggil Java.
- `Menu/Setup.cpp` membebaskan `jclass` hasil `FindClass` sebelum kembali;
  `RegisterNatives` tidak mengambil referensi.

## Anggaran jalur panas

- Proxy hook tidak melakukan alokasi heap, pencarian kelas JNI, atau penulisan
  log per frame. Log hanya ditulis saat nilai slider berubah atau saat aksi
  menu dijalankan.
- `segarkanDaftar()` memakai array stack berukuran tetap dan hanya menyusun
  salinan selama dua detik setelah pembacaan menu terakhir.
- `detakAksi()` mengambil satu aksi per 0,25 detik; antrean berbatas delapan dan
  tidak pernah memblokir thread UI.
- `saatUpdatePemain()` memakai `layaniPosisi()` yang hanya membaca jam monotonik
  dan satu atomik.
- `salinDaftar()`/`salinPosisi()` berjalan di thread UI, bukan di jalur frame
  game.

## Batas yang diketahui

- Thread toast yang menjalankan `Looper.loop()` sengaja tidak pernah kembali,
  jadi thread itu tetap terpasang sampai proses berakhir. Ini perilaku lama dan
  bukan kebocoran referensi lokal per panggilan.
- TSan tidak bisa dijalankan di proses Unity/IL2CPP pada perangkat. Pemeriksaan
  dilakukan dengan membaca tabel di atas dan uji on-device manual.
- `pathFileLog()` belum dipakai; bila nanti dipanggil dari thread lain, akses
  `pathSesi` perlu kunci atau salinan atomik.
- Penangan sinyal hanya menulis `fdLog`; ia tidak mengambil mutex, `LocalRef`,
  atau JNI.
