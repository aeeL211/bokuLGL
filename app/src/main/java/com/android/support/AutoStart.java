package com.android.support;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.provider.Settings;

/**
 * Starts the menu whenever Android creates the game process. A game activity
 * restored after the process was killed in the background does not go through
 * {@link MainActivity}, so without this provider the native library and the
 * overlay service would stay off until the next cold start.
 */
public class AutoStart extends ContentProvider {
    @Override
    public boolean onCreate() {
        Context context = getContext();
        if (context == null)
            return false;
        // The permission screen is owned by MainActivity. Starting the overlay
        // before the permission is granted would only crash the service.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M && !Settings.canDrawOverlays(context))
            return false;
        Main.Start(context);
        return true;
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs,
            String sortOrder) {
        return null;
    }

    @Override
    public String getType(Uri uri) {
        return null;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        return null;
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {
        return 0;
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {
        return 0;
    }
}
