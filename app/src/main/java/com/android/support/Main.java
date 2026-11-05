package com.android.support;

import android.app.Activity;
import android.content.Context;
import android.widget.Toast;

public class Main {

    //Load lib
    static {
        // Must match the CMake target and the name the target table is bound to.
        System.loadLibrary("boku");
    }

    private static native void CheckOverlayPermission(Context context);

    public static void StartWithoutPermission(Context context) {
        CrashHandler.init(context);
        if (context instanceof Activity) {
            //Check if context is an Activity.
            Menu menu = new Menu(context);
            menu.SetWindowManagerActivity();
            menu.ShowMenu();
        } else {
            Toast.makeText(context, "Failed to launch the mod menu\n", Toast.LENGTH_LONG).show();
        }
    }

    public static void Start(Context context) {
        CrashHandler.init(context);
        CheckOverlayPermission(context);
    }
}