#pragma once

// Pencatatan ke berkas bersifat opsional: -DBOKU_LOG=OFF membuang setiap panggilan
// log dan penangan crash, yang menghasilkan .so paling kecil.
#ifdef BOKU_LOG

void printLog(char tingkat, const char* fmt, ...);
void openLogFile();
void installCrashLog();

// Memotret ulang daftar modul tanpa memasang penangan lagi. Panggil setelah
// pustaka game selesai dimuat, supaya nama modul muncul di blok crash.
void refreshModules();

// Mencatat pemanggil abort() dan __stack_chk_fail() supaya SIGABRT berikutnya
// menyebut kode yang memutuskan mematikan proses. Panggil setelah ShadowHook siap.
void installCrashProbe();

// Path lengkap berkas yang ditulis sesi ini, atau nullptr sebelum dibuka.
const char* logFilePath();

#define LOGI(...) printLog('I', __VA_ARGS__)
#define LOGE(...) printLog('E', __VA_ARGS__)

#else

inline void openLogFile() {}
inline void installCrashLog() {}
inline void refreshModules() {}
inline void installCrashProbe() {}
inline const char* logFilePath() { return nullptr; }

#define LOGI(...) ((void)0)
#define LOGE(...) ((void)0)

#endif
