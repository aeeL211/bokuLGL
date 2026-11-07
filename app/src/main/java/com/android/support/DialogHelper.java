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
 * Dialog dengan tautan yang dipanggil dari native. Tampilannya memakai
 * {@link DialogMenu}, sama dengan semua dialog lain di menu.
 */
public class DialogHelper {
    private static final long DELAY_COUNT_MS = 1000L;

    public static void showDialogWithLink(final Context context, final String title,
                                          final String message, final String LinkBtnTitle,
                                          final String CloseBtnTitle, final int sec,
                                          final String url) {
        if (context == null)
            return;
        // Dialog hanya boleh dibuat di thread utama.
        if (Looper.myLooper() != Looper.getMainLooper()) {
            new Handler(Looper.getMainLooper()).post(new Runnable() {
                @Override
                public void run() {
                    showDialogWithLink(context, title, message, LinkBtnTitle, CloseBtnTitle, sec, url);
                }
            });
            return;
        }

        MenuDialog menu = new MenuDialog(context);
        // Context selain Activity (mis. Service) hanya bisa menampilkan dialog lewat jendela overlay.
        menu.setOverlay(!(context instanceof Activity));

        MenuDialog.Button tautan = null;
        if (url != null) {
            tautan = new MenuDialog.Button(LinkBtnTitle, new Runnable() {
                @Override
                public void run() {
                    openUrlSafely(context, url);
                }
            });
        }
        final String labelClose = CloseBtnTitle == null ? "" : CloseBtnTitle;
        String labelInitial = sec > 0 ? labelClose + "(" + sec + ")" : labelClose;

        MenuDialog.Result result = menu.display(title, message, null, null, tautan, null,
                new MenuDialog.Button(labelInitial, null), true);
        if (result != null && sec > 0)
            startCountdown(result, labelClose, sec);
    }

    /** Tombol tutup menghitung mundur dan menekan dirinya sendiri saat habis. */
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
                Button close = result.negatif;
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
        // Dialog yang ditutup lebih awal tidak boleh meninggalkan callback yang menggantung.
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