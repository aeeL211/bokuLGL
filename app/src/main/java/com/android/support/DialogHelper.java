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
    private static final long JEDA_HITUNG_MS = 1000L;

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

        DialogMenu menu = new DialogMenu(context);
        // Context selain Activity (mis. Service) hanya bisa menampilkan dialog lewat jendela overlay.
        menu.setOverlay(!(context instanceof Activity));

        DialogMenu.Tombol tautan = null;
        if (url != null) {
            tautan = new DialogMenu.Tombol(LinkBtnTitle, new Runnable() {
                @Override
                public void run() {
                    openUrlSafely(context, url);
                }
            });
        }
        final String labelTutup = CloseBtnTitle == null ? "" : CloseBtnTitle;
        String labelAwal = sec > 0 ? labelTutup + "(" + sec + ")" : labelTutup;

        DialogMenu.Hasil hasil = menu.tampilkan(title, message, null, null, tautan, null,
                new DialogMenu.Tombol(labelAwal, null), true);
        if (hasil != null && sec > 0)
            mulaiHitungMundur(hasil, labelTutup, sec);
    }

    /** Tombol tutup menghitung mundur dan menekan dirinya sendiri saat habis. */
    private static void mulaiHitungMundur(final DialogMenu.Hasil hasil, final String label,
                                          final int detik) {
        final Handler handler = new Handler(Looper.getMainLooper());
        final Runnable hitung = new Runnable() {
            int sisa = detik;

            @SuppressLint("SetTextI18n")
            @Override
            public void run() {
                if (!hasil.dialog.isShowing())
                    return;
                Button tutup = hasil.negatif;
                if (tutup == null)
                    return;
                if (sisa > 0) {
                    tutup.setText(label + "(" + sisa + ")");
                    sisa--;
                    handler.postDelayed(this, JEDA_HITUNG_MS);
                } else {
                    tutup.performClick();
                }
            }
        };
        // Dialog yang ditutup lebih awal tidak boleh meninggalkan callback yang menggantung.
        hasil.dialog.setOnDismissListener(new DialogInterface.OnDismissListener() {
            @Override
            public void onDismiss(DialogInterface dialog) {
                handler.removeCallbacks(hitung);
            }
        });
        handler.post(hitung);
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