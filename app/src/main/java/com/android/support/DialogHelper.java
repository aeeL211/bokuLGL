package com.android.support;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.widget.Button;
import android.widget.Toast;

/**
 * Link dialog called from native. It is shown with {@link MenuDialog}, the same
 * as every other dialog in the menu.
 */
public class DialogHelper {
    private static final long DELAY_COUNT_MS = 1000L;

    public static void showDialogWithLink(final Context context, final String title,
                                          final String message, final String linkBtnTitle,
                                          final String closeBtnTitle, final int sec,
                                          final String url) {
        if (context == null)
            return;
        // The dialog may only be created on the main thread.
        if (Looper.myLooper() != Looper.getMainLooper()) {
            new Handler(Looper.getMainLooper()).post(new Runnable() {
                @Override
                public void run() {
                    showDialogWithLink(context, title, message, linkBtnTitle, closeBtnTitle, sec, url);
                }
            });
            return;
        }

        MenuDialog menu = new MenuDialog(context);
        // A non-Activity Context (e.g. a Service) can only show a dialog through an overlay window.
        menu.setOverlay(!(context instanceof Activity));

        MenuDialog.Action link = null;
        if (url != null) {
            link = new MenuDialog.Action(linkBtnTitle, new Runnable() {
                @Override
                public void run() {
                    openUrlSafely(context, url);
                }
            });
        }
        final String closeLabel = closeBtnTitle == null ? "" : closeBtnTitle;
        String initialLabel = sec > 0 ? closeLabel + "(" + sec + ")" : closeLabel;

        MenuDialog.Result result = menu.display(title, message, null, null, link, null,
                new MenuDialog.Action(initialLabel, null), true);
        if (result != null && sec > 0)
            startCountdown(result, closeLabel, sec);
    }

    /** The close button counts down and clicks itself when it reaches zero. */
    private static void startCountdown(final MenuDialog.Result result, final String label,
                                          final int second) {
        final Handler handler = new Handler(Looper.getMainLooper());
        final Runnable compute = new Runnable() {
            int remaining = second;

            @SuppressLint("SetTextI18n")
            @Override
            public void run() {
                if (!result.dialog.isShowing())
                    return;
                Button close = result.negative;
                if (close == null)
                    return;
                if (remaining > 0) {
                    close.setText(label + "(" + remaining + ")");
                    remaining--;
                    handler.postDelayed(this, DELAY_COUNT_MS);
                } else {
                    close.performClick();
                }
            }
        };
        // A dialog that is closed early must not leave a dangling callback.
        result.dialog.setOnDismissListener(new DialogInterface.OnDismissListener() {
            @Override
            public void onDismiss(DialogInterface dialog) {
                handler.removeCallbacks(compute);
            }
        });
        handler.post(compute);
    }

    @SuppressLint("QueryPermissionsNeeded")
    public static void openUrlSafely(Context context, String url) {
        if (url == null || url.trim().isEmpty()) {
            return;
        }

        if (!url.startsWith("http://") && !url.startsWith("https://")) {
            url = "http://" + url;
        }

        try {
            Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

            if (intent.resolveActivity(context.getPackageManager()) != null) {
                context.startActivity(intent);
            } else {
                Intent browserIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
                browserIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                browserIntent.setPackage("com.android.chrome");

                if (browserIntent.resolveActivity(context.getPackageManager()) != null) {
                    context.startActivity(browserIntent);
                } else {
                    browserIntent.setPackage(null);
                    context.startActivity(browserIntent);
                }
            }
        } catch (Exception e) {
            Toast.makeText(context, e.toString(), Toast.LENGTH_SHORT).show();
        }
    }
}
