package com.android.support;

import android.app.ActivityManager;
import android.app.Service;
import android.content.Intent;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.view.View;

public class Launcher extends Service {
    /** Selang pemeriksaan apakah game masih di depan. */
    private static final long CHECK_MS = 1000L;

    Menu menu;

    private Handler handler;
    private final Runnable pemeriksa = new Runnable() {
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
        handler.post(pemeriksa);
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    //Check if we are still in the game. If now our menu and menu button will dissapear
    private boolean isNotInGame() {
        ActivityManager.RunningAppProcessInfo runningAppProcessInfo = new ActivityManager.RunningAppProcessInfo();
        ActivityManager.getMyMemoryState(runningAppProcessInfo);
        return runningAppProcessInfo.importance != ActivityManager.RunningAppProcessInfo.IMPORTANCE_FOREGROUND;
    }

    private void checkGame() {
        if (menu == null)
            return;
        menu.setVisibility(isNotInGame() ? View.INVISIBLE : View.VISIBLE);
    }

    //Destroy our View
    @Override
    public void onDestroy() {
        // Pemeriksa dihentikan lebih dulu; kalau tidak, handler terus berjalan
        // dan menahan Service ini di memori setelah dihancurkan.
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
