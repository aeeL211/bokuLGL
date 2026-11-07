package com.android.support;

import android.app.ActivityManager;
import android.app.Service;
import android.content.Intent;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.util.Log;
import android.view.View;

public class Launcher extends Service {
    private static final String TAG = BKLog.TAG;
    /** Interval for checking whether the game is still in the foreground. */
    private static final long CHECK_MS = 1000L;

    Menu menu;

    private Handler handler;
    private final Runnable checker = new Runnable() {
        @Override
        public void run() {
            checkGame();
            if (handler != null)
                handler.postDelayed(this, CHECK_MS);
        }
    };

    //When this Class is called the code in this function will be executed
    @Override
    public void onCreate() {
        super.onCreate();

        menu = new Menu(this);
        menu.SetWindowManagerWindowService();
        menu.ShowMenu();

        //Create a handler for this Class
        handler = new Handler(Looper.getMainLooper());
        handler.post(checker);
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    // The menu belongs to the game, so it is only shown while this process is
    // the one the user is looking at.
    private boolean isGameVisible() {
        ActivityManager.RunningAppProcessInfo info = new ActivityManager.RunningAppProcessInfo();
        ActivityManager.getMyMemoryState(info);
        // Some ROMs report a visible game as IMPORTANCE_VISIBLE instead of
        // FOREGROUND, for example in split screen. Both mean the game is on
        // screen and the menu must stay available.
        return info.importance == ActivityManager.RunningAppProcessInfo.IMPORTANCE_FOREGROUND
                || info.importance == ActivityManager.RunningAppProcessInfo.IMPORTANCE_VISIBLE;
    }

    private void checkGame() {
        if (menu == null)
            return;
        try {
            menu.setVisibility(isGameVisible() ? View.VISIBLE : View.INVISIBLE);
        } catch (Throwable t) {
            // A dead window token must not stop the checker for later ticks.
            Log.d(TAG, "Menu visibility failed: " + t);
        }
    }

    //Destroy our View
    @Override
    public void onDestroy() {
        // The checker is stopped first; otherwise the handler keeps running
        // and holds this Service in memory after it is destroyed.
        if (handler != null) {
            handler.removeCallbacksAndMessages(null);
            handler = null;
        }
        super.onDestroy();
        if (menu != null) {
            menu.onDestroy();
            menu = null;
        }
    }

    //Same as above so it wont crash in the background and therefore use alot of Battery life
    @Override
    public void onTaskRemoved(Intent intent) {
        super.onTaskRemoved(intent);
        stopSelf();
    }

    //Override our Start Command so the Service doesnt try to recreate itself when the App is closed
    @Override
    public int onStartCommand(Intent intent, int i, int i2) {
        return Service.START_NOT_STICKY;
    }
}
