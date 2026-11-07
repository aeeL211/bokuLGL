# Asumsi refaktor modul native boku

Dokumen ini mencatat pembacaan yang dipakai saat refaktor. Tidak ada pertanyaan
balik ke pemilik proyek; pilihan di bawah diambil dari kode dan kontrak yang ada.

## Lingkup

1. Refaktor hanya menyentuh `app/src/main/jni/**`, `docs/**`, dan daftar sumber
   di `CMakeLists.txt`. Berkas Java, layout, resource, `assets/fonts/itim.ttf`,
   `targets.def`, dan isi `ShadowHook/` tidak diubah.
2. Perubahan Java yang belum di-commit di working tree adalah pekerjaan agent
   lain. Perubahan itu tidak di-stage, tidak di-commit, dan tidak dibaca sebagai
   bagian dari kontrak C++ selain untuk memastikan nama method JNI tetap sama.
3. Tidak ada fitur, target hook, atau jalur komunikasi baru. Semua target dan
   urutan pemasangan dipertahankan.

## Baseline

4. Proyek memakai `RegisterNatives` di `JNI_OnLoad`, bukan simbol
   `Java_com_android_support_*`. Baseline JNI karena itu mencatat tabel
   RegisterNatives. `libboku.so` di `artifacts/bokuLGL/` dipakai untuk memastikan
   `.dynsym` memang tidak mengekspor `Java_*` dan tetap mengekspor `JNI_OnLoad`.
5. Baseline hook dihitung dari call graph sumber: `boku/main.cpp` memanggil
   `pasangHookX()`, lalu setiap fungsi memanggil `pasangHook(target(id))`.
   `pasangAkhirMakan<T>` mengembang menjadi delapan target. Hasilnya 73 hook
   runtime dari 66 call site. `README.md`/`AGENTS.md` masih menulis 66; selisih
   ini dicatat di `docs/TEMUAN.md` dan tidak diubah karena bukan perilaku hook.
6. Baseline daftar fitur diambil dari literal `OBFUSCATE("...")` di
   `GetFeatureList()` dan `SettingsList()`. `OBFUSCATE` hanya mengenkripsi literal
   saat kompilasi dan mengembalikan byte yang sama saat runtime, jadi perbandingan
   literal adalah perbandingan isi.
7. Baseline fitur diambil dari `switch (featNum)` di `Changes()`. Nomor yang
   tidak punya `case` tetap diabaikan.

## Perilaku yang dipertahankan

8. `Init` adalah method instance. Argumen kedua fungsi C++ adalah `jobject thiz`;
   `RegisterNatives` mencocokkan nama dan signature, bukan status static Java.
9. `Icon()` sengaja memakai base64 baru dari
   `/storage/emulated/0/Download/ikon_bunga_512_v3_base64.txt` (PNG 512x512,
   39.316 karakter base64) sesuai permintaan pengguna. Ini satu-satunya perubahan
   tampilan; mipmap peluncur, layout, dan berkas Java tidak disentuh.
   `IconWebViewData()` tetap mengembalikan `nullptr` seperti baseline, walaupun
   kontrak Java menyebut string kosong; Java yang sekarang sudah menangani
   `null`. Perubahan ini sengaja tidak dilakukan dan dicatat di
   `docs/TEMUAN.md`.
10. `Changes()` tetap idempoten. Nilai yang sama boleh dikirim berkali-kali, dan
    pemanggilan sebelum game siap hanya menulis state atomik.
11. `DaftarPemain()` tetap `nama<TAB>kunci<TAB>peran`, dipisah `\n`, dengan peran
    `host`, `kamu`, `tamu`. `PosisiPemain()` tetap empat bagian
    `tileX<TAB>tileY<TAB>duniaX<TAB>duniaY`, dengan bagian tile kosong bila game
    tidak menyediakannya.
12. `TendangPemain`/`SalinSkinPemain` mengembalikan `true` hanya bila aksi masuk
    antrean. Antrean tetap berbatas 8 dan tidak memblokir thread UI.
13. Bug dan perilaku aneh yang sudah ada tidak diperbaiki: `Toast` ganda di
    `CheckOverlayPermission`, `IconWebViewData` null, dan `exit(0)` setelah
    permintaan izin overlay. Semuanya dicatat di `docs/TEMUAN.md`.

## Thread

14. "Thread UI" adalah thread utama Android yang memanggil method native Menu dan
    Preferences. "Thread game" adalah thread Unity/IL2CPP yang menjalankan hook.
    Thread pekerja mod hanya melakukan scan dan pemasangan hook saat start.
15. Hook weapon/feces/player dianggap berjalan di satu thread game yang sama.
    Cache irama dan kalibrasi karena itu tidak butuh kunci; state yang benar-benar
    dibagi dengan thread UI memakai atomik atau mutex pendek.
16. Field IL2CPP (`Field`) dan tabel target dianggap immutable setelah scan dan
    pemasangan hook selesai. Tidak ada thread UI yang menulisnya.
17. JNIEnv tidak pernah disimpan. Setiap thread yang butuh JNI memakai
    `EnvScope` yang melakukan attach dan detach berpasangan.
18. TSan tidak tersedia untuk proses Unity/IL2CPP di perangkat. Pemeriksaan
    thread dilakukan dengan membaca kode dan mencatat setiap state bersama di
    `docs/THREADING.md`; pengujian TSan penuh menjadi item manual.

## Build dan uji

19. Tidak ada NDK/kompilator lokal di Termux. Kompilasi dan verifikasi ELF
    dijalankan di GitHub Actions. Uji on-device (log `BK_Menu`, CheckJNI,
    simulasi hook gagal) disiapkan sebagai langkah manual karena hanya pengguna
    yang bisa menjalankan game dan membaca direktori lognya.
20. Perbandingan daftar fitur dan urutan hook dilakukan dengan parser sumber
    (`boku/tools/verify_refactor.py`) yang dijalankan lokal, lalu build CI
    memastikan sumber benar-benar dikompilasi.
