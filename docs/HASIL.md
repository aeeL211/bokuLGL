# Hasil refaktor native bokuLGL

## Ringkasan

Refaktor memisahkan lapisan native menjadi batas yang jelas tanpa mengubah
kontrak Java, nomor fitur, daftar fitur, urutan hook, atau efek hook:

- JNI bridge dan cache kelas/metode dipisahkan dari UI menu.
- Daftar fitur dipindahkan dari `Main.cpp` ke `Menu/FeatureList.cpp`.
- `Changes()` memakai konstanta bernama di `boku/feature_registry.*`.
- Daftar pemain, antrean aksi, dan salinan posisi dipindahkan ke
  `boku/player_service.*`.
- Pembungkus ShadowHook menjadi `boku/hook_manager.*`.
- Semua jalur JNI memakai `EnvScope`/`LocalRef` dan membersihkan eksepsi.
- State lintas thread memakai atomik atau mutex pendek; jalur frame tidak
  menambah alokasi, JNI, atau log.

Dua perubahan yang diminta pengguna ikut masuk:

1. `Icon()` memakai base64 baru dari
   `/storage/emulated/0/Download/ikon_bunga_512_v3_base64.txt`.
2. Tag log native berubah dari `Mod_Menu` menjadi `BK_Menu`.

## Peta berkas lama ke baru

| Sebelum | Sesudah |
|---|---|
| `app/src/main/jni/Main.cpp` (daftar fitur, wrapper JNI, `Changes`) | `Menu/FeatureList.cpp`, `Menu/Menu.cpp`, `boku/feature_registry.cpp` |
| `app/src/main/jni/boku/hook.cpp` | `app/src/main/jni/boku/hook_manager.cpp` |
| `app/src/main/jni/boku/hook.h` | `app/src/main/jni/boku/hook_manager.h` |
| Daftar pemain + antrean aksi di `boku/hooks/room.cpp` | `boku/player_service.cpp/.h` |
| Salinan posisi di `boku/hooks/pemain.cpp` | `boku/player_service.cpp/.h` |
| `FindClass`/`GetMethodID` berulang di `Menu/Jni.cpp`, `boku/info.cpp`, `boku/toast.cpp` | `boku/jni_cache.cpp/.h` |
| `lekatJni`/`DetachCurrentThread` manual | `boku/jni_util.cpp/.h` (`EnvScope`, `LocalRef`, `StringUtf`) |
| `timespec`/`clock_gettime` di beberapa hook | `boku/waktu.h` |
| Daftar sumber `Main.cpp` + `hook.cpp` di `CMakeLists.txt` | `Menu/FeatureList.cpp`, `hook_manager.cpp`, `jni_cache.cpp`, `jni_util.cpp`, `player_service.cpp`, `feature_registry.cpp` |
| Baseline dan catatan refaktor | `docs/baseline/*`, `docs/ASUMSI.md`, `docs/THREADING.md`, `docs/TEMUAN.md`, `docs/HASIL.md` |

## Verifikasi lokal

Perintah:

```bash
python3 app/src/main/jni/boku/tools/verify_refactor.py
```

Keluaran terakhir:

```text
OK   daftar GetFeatureList: 30 entri identik
OK   daftar SettingsList: 7 entri identik
OK   tabel JNI: 12 metode di 3 kelas identik
OK   urutan hook: 73 pemasangan runtime identik
```

Pemeriksaan tambahan:

- `docs/baseline/baseline_fitur.txt` dibandingkan manual dengan
  `boku/feature_registry.cpp`: nomor 0, 1, 2, 4, 5, 6, 7, 9, 11, 12, 13, 14,
  15, 16, 19, 20, 21, 22, 23, 24, 25, 26, 27, dan 28 memiliki efek yang sama;
  nomor 3, 8, 10, 17, 18 dan nilai negatif tetap jatuh ke default.
- Nilai awal di `boku/fitur.h` sama dengan baseline.
- `IconWebViewData()` tetap `nullptr`; toast ganda dan `exit(0)` tetap ada.
- `git diff --stat` untuk `app/src/main/java` tidak pernah di-stage.

## Status CI

Diisi setelah build GitHub Actions untuk commit refaktor:

- Build: `(menunggu)`
- APK: `(menunggu)`
- Format (non-blocking): `(menunggu)`

## Kriteria selesai

| Kriteria | Status | Bukti |
|---|---|---|
| Build CI bersih | Menunggu | Akan diisi dari run GitHub Actions |
| `libboku.so` tetap satu-satunya pustaka native | Menunggu | CMake masih membangun `boku` sebagai satu shared library; ShadowHook statis |
| Tabel JNI sama | Lulus lokal | 12 metode di 3 kelas, `verify_refactor.py` |
| Urutan hook sama | Lulus lokal | 73 pemasangan runtime, `verify_refactor.py` |
| Daftar fitur byte-identik | Lulus lokal | 30 + 7 entri, `verify_refactor.py` |
| Efek fitur sama | Lulus lokal | Perbandingan `baseline_fitur.txt` dengan `feature_registry.cpp` |
| Hook gagal tidak mematikan proses | Lulus audit sumber | `pasangHook` mencatat errno; proxy memeriksa `orig` sebelum memanggil |
| Tidak ada alokasi/JNI/log per frame | Lulus audit sumber | `THREADING.md`; jalur frame hanya membaca atomik dan salinan stack |
| Tidak ada data race | Lulus audit sumber | Tabel state di `THREADING.md`; TSan manual belum dijalankan |
| Tidak ada kebocoran referensi lokal | Lulus audit sumber | `LocalRef`/`StringUtf`; dua jalur lama di `log.cpp`/`jni_bridge.cpp` menghapus manual |
| Tidak ada perubahan Java/resource | Lulus | Working tree Java tidak di-stage; hanya native/docs yang di-commit |
| Feature ID dan signature JNI tetap | Lulus lokal | Baseline JNI + verifier |
| README/AGENTS tidak diubah | Lulus | Tidak ada diff README/AGENTS |

## Uji manual yang masih diperlukan

1. Gabungkan `artifacts/bokuLGL/` ke APK game, pasang, lalu buka game.
2. Baca log terbaru `Android/data/com.pixticle.bokuboku/files/bokumods-*.log`
   dengan MT Manager. Cari `ShadowHook siap`, `Pemantau dlopen tidak dipakai`,
   `Scan selesai: 86 dari 86 metode ditemukan`, `Field ... siap`, `Hook gagal:`,
   dan `Hook terpasang:`.
3. Buka menu, periksa ikon bunga baru, lalu uji setiap section dan slider.
4. Uji `DaftarPemain`/`PosisiPemain` di room, tendang tamu, dan salin skin.
5. Paksa satu hook gagal (misalnya dengan mengubah sementara nama target di
   build uji) dan pastikan game tetap berjalan serta log mencatat
   `Hook gagal: ... ShadowHook <kode>`. Jangan memakai build uji itu untuk
   rilis.
6. Jalankan dengan CheckJNI bila memungkinkan dan pantau log untuk
   `Eksepsi JNI dibersihkan`, `Toast gagal`, atau `Hook gagal:` yang berulang.
7. TSan penuh tidak tersedia untuk proses Unity/IL2CPP; gunakan tabel
   `THREADING.md` sebagai pengganti audit.

## Risiko yang tersisa

- Base64 ikon 39.316 karakter adalah literal besar. Bila kompilator mencapai
  batas langkah `constexpr`, literal harus dipecah menjadi beberapa potongan.
- Thread toast yang menjalankan `Looper.loop()` tetap terpasang sampai proses
  berakhir; ini perilaku lama.
- Tag log Java masih `Mod_Menu`; hanya tag native yang berubah menjadi
  `BK_Menu`. Lihat `docs/TEMUAN.md`.
- Jumlah hook di `README.md`/`AGENTS.md` masih 66, sedangkan runtime mencetak
  73. Tidak diubah karena di luar refaktor perilaku.
- Uji on-device dan TSan belum dilakukan di sesi ini.
