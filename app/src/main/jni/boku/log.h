#pragma once

// File logging is optional: -DBOKU_LOG=OFF drops every log call and the crash
// handler, producing the smallest .so.
#ifdef BOKU_LOG

void printLog(char level, const char* fmt, ...);
void openLogFile();
void installCrashLog();

// Re-captures the module list without installing the handler again. Call after
// the game libraries finish loading, so module names appear in the crash block.
void refreshModules();

// Records the caller of abort() and __stack_chk_fail() so the next SIGABRT names
// the code that decided to kill the process. Call after ShadowHook is ready.
void installCrashProbe();

// Full path of the file written by this session, or nullptr before it is opened.
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
