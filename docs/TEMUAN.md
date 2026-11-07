# Temuan yang tidak diubah

Refaktor ini menjaga perilaku. Temuan di bawah sengaja dicatat, bukan diperbaiki,
karena memperbaikinya akan mengubah kontrak Java, fitur, atau cakupan tugas.

## Kontrak dan dokumentasi

1. **Jumlah hook di dokumentasi tidak sama dengan runtime.** `README.md` dan
   `AGENTS.md` menyebut `Hook terpasang: 66`. Sumber sebenarnya memasang 73 hook
   runtime: 65 pemanggilan `pasangHook(target(...))` langsung ditambah
   `pasangAkhirMakan<T>` yang mengembang menjadi delapan target. Angka 66 adalah
   jumlah call site (`pasangHook`), bukan jumlah hook yang terpasang. Log runtime
   tetap mencetak `Hook terpasang: 73`; dokumentasi tidak diubah karena bukan
   perilaku hook dan berada di luar refaktor native.
2. **Tag log Java masih `Mod_Menu`.** `Menu.TAG`, `IkonCache.TAG`, dan
   `Theme.java` masih memakai tag lama. Tag native sudah `BK_Native` sesuai
   permintaan; penyeragaman tag Java diserahkan ke agent Java.
3. **`IconWebViewData()` mengembalikan `nullptr`.** Kontrak Java menyebut string,
   tetapi baseline juga mengembalikan `nullptr`. Java yang sekarang sudah
   menangani nilai null; perilaku ini dipertahankan.
4. **Toast ganda di `CheckOverlayPermission`.** Baseline menampilkan pesan izin
   overlay dua kali. Dipertahankan agar alur izin tidak berubah.
5. **`exit(0)` setelah permintaan izin overlay.** Baseline mematikan proses lima
   detik setelah membuka halaman izin. Dipertahankan; ini keputusan produk, bukan
   bug refaktor.
6. **Nomor fitur 3, 8, 10, 17, dan 18 tidak punya `case` di `Changes`.** Menu
   tidak memakai nomor itu, dan baseline mengabaikannya. Nomor yang ada tidak
   digeser.

## Perilaku runtime yang sudah ada

7. **Thread toast sengaja hidup terus.** Bila pemanggil tidak punya Looper,
   `threadToast` menyiapkan Looper lalu menjalankan `Looper.loop()` yang tidak
   pernah kembali. Thread tetap terpasang sampai proses berakhir. Ini perilaku
   lama; jalur gagal sekarang melepas thread bila `Looper.loop` tidak tersedia.
8. **`Dialog()` tidak dipakai saat ini.** Helper ini tetap disimpan karena
   kontrak template, tetapi tidak ada pemanggil di sumber sekarang. Parameter
   `url` yang null akan tetap menjadi kesalahan pemanggil.
9. **`Icon()` membawa literal base64 39.316 karakter.** Ini permintaan pengguna,
   bukan temuan refaktor. `OBFUSCATE` mengevaluasi salinan dan XOR saat
   kompilasi; bila suatu saat kompilator mencapai batas langkah `constexpr`,
   literal perlu dipecah menjadi beberapa potongan tanpa mengubah isinya.
10. **Pemasangan hook tidak bisa dibatalkan saat game berjalan.** `pasangHook`
    hanya menambah entri; tidak ada `unhook` di jalur fitur. Ini kontrak
    `AGENTS.md` dan tidak diubah.
11. **Jendela `Target::orig` saat pemasangan.** Proxy hanya dipanggil setelah
    ShadowHook mengaktifkan patch, dan setiap proxy memeriksa `orig` sebelum
    memanggilnya. Bila `orig` kosong, hook membiarkan perilaku vanilla. Tidak ada
    mekanisme tambahan yang diperlukan untuk pemasangan sekali di awal.
12. **`pathFileLog()` belum dipakai.** Fungsinya mengembalikan `pathSesi` tanpa
    kunci. Selama tidak ada pemanggil baru, tidak ada pembaca yang berlomba
    dengan `bukaFileLog`; bila nanti dipakai dari thread lain, aksesnya perlu
    disinkronkan.
13. **`Menu/*.cpp` dikompilasi dengan `-w`.** Ini pengaturan CMake yang sudah ada
    untuk sumber template. Sumber mod di `boku/**` tetap memakai
    `-Wall -Wextra -Wshadow -Werror`. Tidak diubah supaya sinyal warning tidak
    berubah.
14. **ShadowHook tetap memakai patch lokal `LIBBOKU PATCH`.** Patch di
    `ShadowHook/sh_linker.c` melewati pemantau dlopen karena
    `libshadowhook_nothing.so` tidak dipaketkan. Saat versi ShadowHook dinaikkan,
    patch harus dipasang ulang; baris log `Pemantau dlopen tidak dipakai` tetap
    ada.
15. **Perubahan UI Java di-commit terpisah.** Berkas seperti `Menu.java`,
    `Theme.java`, dan `IkonFitur.java` masuk pada commit `7fc107b`, bukan di
    commit refaktor native. Verifikasi JNI memastikan nama dan signature Java
    yang sudah di-commit tetap cocok.

## Keterbatasan pengujian

16. **Tidak ada NDK, kompilator, `adb`, atau TSan di Termux.** Kompilasi dan
    pemeriksaan ELF harus lewat GitHub Actions. Uji on-device (CheckJNI, hook
    gagal paksa, log `BK_Native`) harus dijalankan pengguna karena hanya pengguna
    yang bisa membuka game dan membaca direktori lognya.
17. **`docs/` berada di dalam repo `bokuLGL/`.** Ini mengikuti struktur repo;
    workspace root tidak memiliki folder `docs/`.
