package com.android.support;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.widget.Toast;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.lang.Thread.UncaughtExceptionHandler;
import java.nio.charset.StandardCharsets;
import java.text.SimpleDateFormat;
import java.util.Arrays;
import java.util.Date;
import java.util.Locale;
import java.util.Map;

public final class CrashHandler {

    private static final String TAG = BKLog.TAG;
    private static final String LOG_DIR = "crash_logs";
    private static final String FILE_PREFIX = "crash_";
    private static final String FILE_EXTENSION = ".txt";
    private static final int LOG_SAVED = 5;
    private static final long TOAST_SHOW_MS = 2000L;
    private static final long WAIT_MAX_MS = 2500L;

    private static UncaughtExceptionHandler defaultHandler;
    private static Context appContext;
    private static volatile boolean isHandling;
    final static boolean showToasts = true;

    private CrashHandler() {
    }

    /// Due to changes in access to application folders and to maintain convenience, the path has been changed to: android/media/PACKAGE/files/LOG_DIR.
    /// This path should be accessible on all devices
    public static synchronized void init(final Context context) {
        if (context == null)
            return;
        appContext = context.getApplicationContext();

        // Called twice (MainActivity then Main) it must not wrap our own
        // handler, so the handler chain does not keep growing.
        UncaughtExceptionHandler now = Thread.getDefaultUncaughtExceptionHandler();
        if (!(now instanceof CrashCatcher))
            defaultHandler = now;

        cleanOldLogs();

        Thread.setDefaultUncaughtExceptionHandler(new CrashCatcher());
    }

    private static final class CrashCatcher implements UncaughtExceptionHandler {
        @Override
        public void uncaughtException(Thread thread, Throwable throwable) {
            Log.e(TAG, "Uncaught exception detected", throwable);

            // A crash inside crash handling is forwarded straight to the original handler.
            if (isHandling) {
                forward(thread, throwable);
                return;
            }
            isHandling = true;

            try {
                handleException(thread, throwable);
            } catch (Throwable e) {
                Log.e(TAG, "Error in crash handler", e);
                forward(thread, throwable);
            }
        }
    }

    private static void forward(Thread thread, Throwable throwable) {
        if (defaultHandler != null) {
            defaultHandler.uncaughtException(thread, throwable);
        } else {
            android.os.Process.killProcess(android.os.Process.myPid());
            System.exit(10);
        }
    }

    private static void handleException(Thread thread, Throwable throwable) {
        // Save crash log
        File crashFile = saveCrashLog(throwable, thread);

        // Show notification to user. Toast needs a Looper, and the crashing
        // thread (especially the main thread) cannot be used, so the toast runs
        // on a helper thread with its own Looper.
        Thread viewer = null;
        if (showToasts && appContext != null)
            viewer = showToast(crashFile);

        // Wait a bit for toast to show and log to be saved
        if (viewer != null) {
            try {
                viewer.join(WAIT_MAX_MS);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }

        forward(thread, throwable);
    }

    private static File saveCrashLog(Throwable throwable, Thread source) {
        File dir = getLogDirectory();
        if (dir == null)
            return null;

        String timeStamp = new SimpleDateFormat("yyyy_MM_dd-HH_mm_ss", Locale.US)
                .format(new Date());

        String fileName = FILE_PREFIX + timeStamp + FILE_EXTENSION;
        File logFile = new File(dir, fileName);

        try {
            writeCrashLog(logFile, throwable, source);
            Log.i(TAG, "Crash log saved to: " + logFile.getAbsolutePath());
            return logFile;
        } catch (IOException e) {
            Log.e(TAG, "Failed to save crash log", e);
            return null;
        }
    }

    private static void writeCrashLog(File file, Throwable throwable, Thread source)
            throws IOException {
        File parentDir = file.getParentFile();
        if (parentDir != null && !parentDir.exists()) {
            if (!parentDir.mkdirs()) {
                Log.w(TAG, "Failed to create directory: " + parentDir.getAbsolutePath());
            }
        }

        String logContent = buildCrashReport(throwable, source);

        try (FileOutputStream fos = new FileOutputStream(file)) {
            fos.write(logContent.getBytes(StandardCharsets.UTF_8));
            fos.flush();
        }
    }

    private static String buildCrashReport(Throwable throwable, Thread source) {
        StringBuilder report = new StringBuilder(8192);

        // App and device info
        report.append("=============== CRASH REPORT ===============\n");
        report.append("Time: ").append(getCurrentTime()).append("\n");
        report.append("App Version: ").append(getAppVersion()).append("\n");
        report.append("Android API: ").append(Build.VERSION.SDK_INT).append("\n");
        report.append("Android Version: ").append(Build.VERSION.RELEASE).append("\n");
        report.append("Device: ").append(Build.MANUFACTURER).append(" ").append(Build.MODEL).append("\n");
        report.append("Product: ").append(Build.PRODUCT).append("\n");
        report.append("Board: ").append(Build.BOARD).append("\n");
        report.append("Fingerprint: ").append(Build.FINGERPRINT).append("\n");
        // The thread that crashed, not the one writing the report.
        Thread origin = source != null ? source : Thread.currentThread();
        report.append("Thread: ").append(origin.getName()).append("\n");
        report.append("=============== STACK TRACE ===============\n");

        // printStackTrace already prints the whole "Caused by" chain, so there
        // is no need to walk getCause() ourselves (the old version wrote it to a
        // writer whose content had already been taken, so the cause chain never
        // made it in).
        StringWriter sw = new StringWriter();
        PrintWriter pw = new PrintWriter(sw);
        throwable.printStackTrace(pw);
        pw.flush();
        report.append(sw.toString());

        // Thread dump
        report.append("\n\n=============== ALL THREADS ===============\n");
        Map<Thread, StackTraceElement[]> allThreads = Thread.getAllStackTraces();
        for (Map.Entry<Thread, StackTraceElement[]> entry : allThreads.entrySet()) {
            Thread thread = entry.getKey();
            report.append("\nThread: ").append(thread.getName())
                    .append(" [ID: ").append(thread.getId())
                    .append(", State: ").append(thread.getState())
                    .append(", Priority: ").append(thread.getPriority())
                    .append("]\n");

            for (StackTraceElement element : entry.getValue()) {
                report.append("    at ").append(element.toString()).append("\n");
            }
        }

        return report.toString();
    }

    private static File getLogDirectory() {
        if (appContext == null) {
            Log.e(TAG, "CrashHandler is not initialized");
            return null;
        }

        File standardAppDir = appContext.getExternalFilesDir(null);
        if (standardAppDir == null) {
            Log.e(TAG, "Cannot get app directory, using internal storage");
            return new File(appContext.getFilesDir(), LOG_DIR);
        }

        String standardPath = standardAppDir.getAbsolutePath();
        String targetPath = standardPath.replace(
                "Android/data/" + appContext.getPackageName(),
                "Android/media/" + appContext.getPackageName()
        );
        File logDir = new File(targetPath, LOG_DIR);

        if (!logDir.exists()) {
            if (logDir.mkdirs()) {
                Log.i(TAG, "Created log directory: " + logDir.getAbsolutePath());
            } else {
                Log.e(TAG, "Failed to create Android/media directory, falling back to standard location");
                return new File(standardAppDir, LOG_DIR);
            }
        }
        return logDir;
    }

    private static String getDisplayPath(File file) {
        if (file == null) return null;
        String absolutePath = file.getAbsolutePath();
        int androidIndex = absolutePath.indexOf("/Android/");
        if (androidIndex != -1) {
            return absolutePath.substring(androidIndex + 1);
        }
        return file.getName();
    }

    /** Shows the toast on a Looper helper thread for {@link #TOAST_SHOW_MS}. */
    private static Thread showToast(final File crashFile) {
        final String message;
        if (crashFile != null && crashFile.exists()) {
            message = "CRASH! check:\n" + getDisplayPath(crashFile);
        } else {
            message = "CRASH! Could not save log.";
        }

        Thread thread = new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    Looper.prepare();
                    Toast.makeText(appContext, message, Toast.LENGTH_LONG).show();
                    final Looper looper = Looper.myLooper();
                    new Handler(looper).postDelayed(new Runnable() {
                        @Override
                        public void run() {
                            looper.quit();
                        }
                    }, TOAST_SHOW_MS);
                    Looper.loop();
                } catch (Throwable t) {
                    Log.e(TAG, "Failed to show crash toast", t);
                }
            }
        }, "CrashToast");
        thread.start();
        return thread;
    }

    private static String getAppVersion() {
        if (appContext == null) return "unknown";

        try {
            PackageInfo pInfo = appContext.getPackageManager()
                    .getPackageInfo(appContext.getPackageName(), 0);
            long versionCode = Build.VERSION.SDK_INT >= Build.VERSION_CODES.P ?
                    pInfo.getLongVersionCode() : pInfo.versionCode;
            return pInfo.versionName + " (" + versionCode + ")";
        } catch (PackageManager.NameNotFoundException e) {
            return "unknown";
        }
    }

    private static String getCurrentTime() {
        return new SimpleDateFormat("yyyy-MM-dd HH:mm:ss Z", Locale.US)
                .format(new Date());
    }

    /// utility method to manually log an exception
    public static void logException(Throwable throwable) {
        Log.e(TAG, "Manual exception logging", throwable);
        saveCrashLog(throwable, Thread.currentThread());
    }

    /// clear old crash logs (keep last 5)
    public static void cleanOldLogs() {
        File logDir = getLogDirectory();
        if (logDir == null || !logDir.exists() || !logDir.isDirectory()) return;

        File[] logs = logDir.listFiles((dir, name) ->
                name.startsWith(FILE_PREFIX) && name.endsWith(FILE_EXTENSION));

        if (logs != null && logs.length > LOG_SAVED) {
            Arrays.sort(logs, (f1, f2) ->
                    Long.compare(f2.lastModified(), f1.lastModified()));

            // The old version started at index 10, leaving 10 files even though
            // the comment says "keep last 5".
            for (int i = LOG_SAVED; i < logs.length; i++) {
                if (!logs[i].delete()) {
                    Log.w(TAG, "Failed to delete old log: " + logs[i].getName());
                }
            }
        }
    }
}
