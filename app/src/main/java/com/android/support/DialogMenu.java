package com.android.support;

import android.app.Dialog;
import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.os.Build;
import android.text.Html;
import android.text.InputFilter;
import android.text.InputType;
import android.text.TextUtils;
import android.text.method.DigitsKeyListener;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;
import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;

/**
 * Semua dialog menu, dibangun sendiri (bukan AlertDialog bawaan) supaya
 * tampilannya sama di semua versi dan merek Android.
 *
 * Susunannya mengulang panel menu: bingkai cokelat 3dp, pita header berisi
 * judul, tepi bergelombang, isi di atas BODY, tepi bergelombang, lalu footer
 * berisi tombol kayu. Tombol utama (kanan) berbingkai putih seperti kontrol
 * aktif di menu, tombol lain berbingkai krem.
 *
 * Dialog yang sedang terbuka dicatat dan ditutup di {@link #tutup()}, jadi
 * tidak ada dialog yang tertinggal menahan Context setelah menu dihancurkan.
 */
final class DialogMenu {
    private static final String TAG = Menu.TAG;
    private static final int TIPE_OVERLAY = Build.VERSION.SDK_INT >= 26 ? 2038 : 2002;

    /** Lebar maksimum sama dengan lebar menu, dengan sisa tepi layar. */
    private static final float LEBAR_MAKS_DP = 340f;
    private static final float MARGIN_LAYAR_DP = 16f;
    /** Isi yang lebih tinggi dari ini digulir, supaya tombol tidak terdorong keluar layar. */
    private static final float TINGGI_ISI_MAKS = 0.5f;
    private static final float REDUP = 0.5f;
    private static final float TEKS_PESAN_SP = Theme.ROW_SP;

    /** Penerima teks yang dimasukkan pengguna. */
    interface Penerima {
        void diterima(String teks);
    }

    /** Satu tombol: label dan aksi yang dijalankan sebelum dialog ditutup. */
    static final class Tombol {
        final String label;
        final Runnable aksi;

        Tombol(String label, Runnable aksi) {
            this.label = label == null ? "" : label;
            this.aksi = aksi;
        }
    }

    /** Dialog yang tampil beserta tombolnya (null bila tombol itu tidak ada). */
    static final class Hasil {
        final Dialog dialog;
        Button positif;
        Button netral;
        Button negatif;

        Hasil(Dialog dialog) {
            this.dialog = dialog;
        }
    }

    private final Context context;
    private boolean overlay;
    private Dialog terbuka;

    DialogMenu(Context context) {
        this.context = context;
    }

    /** Dialog harus memakai jendela overlay bila menu berjalan sebagai Service. */
    void setOverlay(boolean overlay) {
        this.overlay = overlay;
    }

    /** Menutup dialog yang masih terbuka. Aman dipanggil berulang. */
    void tutup() {
        Dialog dialog = terbuka;
        terbuka = null;
        if (dialog != null && dialog.isShowing()) {
            try {
                dialog.dismiss();
            } catch (Throwable t) {
                Log.d(TAG, "Dialog gagal ditutup: " + t);
            }
        }
    }

    /** Dialog penjelasan yang dibuka dengan menekan lama sebuah fitur. */
    void penjelasan(int featNum, String featName) {
        tampilkan(Html.fromHtml(featName), Penjelasan.untuk(featNum, featName), null, null,
                new Tombol("Mengerti", null), null, null, true);
    }

    /**
     * Dialog dengan satu kolom isian. Enter di keyboard sama dengan menekan OK.
     * Dialog ini tidak ditutup dengan mengetuk di luarnya supaya teks yang
     * sedang diketik tidak hilang tanpa sengaja.
     *
     * @param hint         keterangan di atas kolom (mis. batas nilai), atau null
     * @param angka        true bila hanya boleh angka dan tanda minus
     * @param maksKarakter batas panjang teks, 0 bila tanpa batas
     * @param netral       label tombol ketiga, atau null bila tidak ada
     */
    void input(String judul, String hint, String awal, boolean angka, int maksKarakter,
               final Penerima ok, String netral, Runnable aksiNetral) {
        final EditText isian = new EditText(context);
        isian.setTextColor(Theme.TEXT);
        isian.setTextSize(Theme.ROW_SP);
        isian.setHintTextColor(Theme.MUTED);
        isian.setBackground(Theme.dialogInput(context));
        isian.setMinHeight(dp(Theme.TARGET_DP));
        isian.setPadding(dp(12), dp(8), dp(12), dp(8));
        isian.setSingleLine(true);
        isian.setImeOptions(EditorInfo.IME_ACTION_DONE);
        if (angka) {
            isian.setInputType(InputType.TYPE_CLASS_NUMBER);
            isian.setKeyListener(DigitsKeyListener.getInstance("0123456789-"));
        }
        if (maksKarakter > 0)
            isian.setFilters(new InputFilter[]{new InputFilter.LengthFilter(maksKarakter)});
        if (awal != null && awal.length() > 0) {
            isian.setText(awal);
            isian.setSelection(isian.length());
        }

        LinearLayout isi = new LinearLayout(context);
        isi.setOrientation(LinearLayout.VERTICAL);
        if (hint != null && hint.length() > 0) {
            TextView keterangan = Theme.teks(context, hint, Theme.DESC_SP, Theme.MUTED);
            keterangan.setPadding(dp(2), 0, dp(2), dp(6));
            isi.addView(keterangan, new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));
        }
        isi.addView(isian, new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));

        final Hasil[] hasil = new Hasil[1];
        isian.setOnEditorActionListener(new TextView.OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                if (actionId == EditorInfo.IME_ACTION_DONE && hasil[0] != null
                        && hasil[0].positif != null) {
                    hasil[0].positif.performClick();
                    return true;
                }
                return false;
            }
        });

        Tombol tombolOk = new Tombol("OK", new Runnable() {
            @Override
            public void run() {
                ok.diterima(isian.getText().toString());
            }
        });
        Tombol tombolNetral = netral != null && aksiNetral != null
                ? new Tombol(netral, aksiNetral) : null;
        hasil[0] = tampilkan(judul, null, isi, isian, tombolOk, tombolNetral,
                new Tombol("Batal", null), false);
    }

    /**
     * Membangun dan menampilkan dialog. Mengembalikan null bila gagal tampil
     * (tanpa izin overlay atau Context sudah mati) -- menu tidak boleh ikut
     * crash karena dialog.
     *
     * @param pesan  teks biasa di isi dialog, boleh null
     * @param isi    view tambahan di bawah pesan, boleh null
     * @param fokus  kolom isian yang mendapat fokus dan keyboard, boleh null
     */
    Hasil tampilkan(CharSequence judul, CharSequence pesan, View isi, EditText fokus,
                    Tombol positif, Tombol netral, Tombol negatif, boolean tutupDiLuar) {
        tutup();

        final Dialog dialog = new Dialog(context, android.R.style.Theme_DeviceDefault_Dialog);
        dialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        Hasil hasil = new Hasil(dialog);

        DisplayMetrics dm = context.getResources().getDisplayMetrics();
        int lebar = Math.min(dp(LEBAR_MAKS_DP), dm.widthPixels - 2 * dp(MARGIN_LAYAR_DP));

        // Bingkai luar: isi gelap 3dp, padding view membentuk garis tepi.
        LinearLayout akar = new LinearLayout(context);
        akar.setOrientation(LinearLayout.VERTICAL);
        akar.setBackground(Theme.panel(context));
        akar.setPadding(dp(3), dp(3), dp(3), dp(3));

        TextView kepala = Theme.teks(context, judul == null ? "" : judul, Theme.SECTION_SP,
                Theme.TITLE_TEXT);
        kepala.setGravity(Gravity.CENTER);
        kepala.setMaxLines(2);
        kepala.setEllipsize(TextUtils.TruncateAt.END);
        kepala.setMinHeight(dp(Theme.TARGET_DP));
        kepala.setPadding(dp(14), dp(8), dp(14), dp(8));
        kepala.setBackground(Theme.header(context));
        akar.addView(kepala, new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));

        Theme.Zigzag zigAtas = new Theme.Zigzag(context, Theme.HEADER, true);
        zigAtas.setBackgroundColor(Theme.BODY);
        akar.addView(zigAtas, new LinearLayout.LayoutParams(MATCH_PARENT, dp(5)));

        LinearLayout badan = new LinearLayout(context);
        badan.setOrientation(LinearLayout.VERTICAL);
        badan.setPadding(dp(14), dp(8), dp(14), dp(12));
        if (pesan != null && pesan.length() > 0) {
            TextView teks = Theme.teks(context, pesan, TEKS_PESAN_SP, Theme.TEXT);
            teks.setLineSpacing(dp(2), 1f);
            badan.addView(teks, new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));
        }
        if (isi != null)
            badan.addView(isi, new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));

        GulirMaks gulir = new GulirMaks(context, (int) (dm.heightPixels * TINGGI_ISI_MAKS));
        gulir.setBackgroundColor(Theme.BODY);
        Theme.gayaScrollbar(context, gulir);
        gulir.addView(badan, new ViewGroup.LayoutParams(MATCH_PARENT, WRAP_CONTENT));
        akar.addView(gulir, new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));

        Theme.Zigzag zigBawah = new Theme.Zigzag(context, Theme.FOOTER, false);
        zigBawah.setBackgroundColor(Theme.BODY);
        akar.addView(zigBawah, new LinearLayout.LayoutParams(MATCH_PARENT, dp(5)));

        // Urutan kiri ke kanan: negatif, netral, positif (tombol utama di kanan).
        LinearLayout kaki = new LinearLayout(context);
        kaki.setOrientation(LinearLayout.HORIZONTAL);
        kaki.setBackground(Theme.footer(context));
        kaki.setPadding(dp(8), dp(6), dp(8), dp(6));
        hasil.negatif = tambahTombol(kaki, dialog, negatif, false);
        hasil.netral = tambahTombol(kaki, dialog, netral, false);
        hasil.positif = tambahTombol(kaki, dialog, positif, true);
        akar.addView(kaki, new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));

        Theme.applyFont(akar);
        dialog.setContentView(akar, new ViewGroup.LayoutParams(lebar, WRAP_CONTENT));
        dialog.setCanceledOnTouchOutside(tutupDiLuar);

        Window window = dialog.getWindow();
        if (window != null) {
            window.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
            window.setLayout(lebar, WRAP_CONTENT);
            window.setGravity(Gravity.CENTER);
            window.addFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);
            window.setDimAmount(REDUP);
            if (overlay)
                window.setType(TIPE_OVERLAY);
            if (fokus != null)
                window.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_VISIBLE
                        | WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
        }

        try {
            dialog.show();
        } catch (Throwable t) {
            Log.d(TAG, "Dialog gagal ditampilkan: " + t);
            return null;
        }
        terbuka = dialog;
        Transisi.masuk(akar);
        if (fokus != null)
            munculkanKeyboard(fokus);
        return hasil;
    }

    private Button tambahTombol(LinearLayout kaki, final Dialog dialog, final Tombol tombol,
                                boolean utama) {
        if (tombol == null)
            return null;

        Button button = new Button(context);
        button.setText(tombol.label);
        button.setTextColor(Theme.TITLE_TEXT);
        button.setTextSize(Theme.ROW_SP);
        button.setTypeface(Theme.font(context));
        button.setAllCaps(false);
        button.setSingleLine(true);
        button.setEllipsize(TextUtils.TruncateAt.END);
        button.setGravity(Gravity.CENTER);
        button.setBackground(utama ? Theme.tombolUtama(context) : Theme.tombolTekan(context));
        button.setMinWidth(0);
        button.setMinimumWidth(0);
        button.setMinHeight(dp(Theme.TARGET_DP));
        button.setMinimumHeight(dp(Theme.TARGET_DP));
        button.setPadding(dp(8), 0, dp(8), 0);
        button.setStateListAnimator(null);
        button.setElevation(0f);
        button.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                // Aksi jalan dulu (ia membaca isian), dialog ditutup sesudahnya
                // walau aksinya melempar exception.
                try {
                    if (tombol.aksi != null)
                        tombol.aksi.run();
                } finally {
                    dialog.dismiss();
                }
            }
        });

        LinearLayout.LayoutParams params =
                new LinearLayout.LayoutParams(0, dp(Theme.TARGET_DP), 1f);
        if (kaki.getChildCount() > 0)
            params.leftMargin = dp(6);
        kaki.addView(button, params);
        return button;
    }

    /** Keyboard dimunculkan lewat showSoftInput, bukan toggleSoftInput yang bisa malah menyembunyikannya. */
    private void munculkanKeyboard(final EditText isian) {
        isian.requestFocus();
        isian.post(new Runnable() {
            @Override
            public void run() {
                InputMethodManager imm = (InputMethodManager)
                        context.getSystemService(Context.INPUT_METHOD_SERVICE);
                if (imm != null)
                    imm.showSoftInput(isian, InputMethodManager.SHOW_IMPLICIT);
            }
        });
    }

    /** ScrollView yang tingginya dibatasi, supaya isi panjang tidak mendorong tombol keluar layar. */
    private static final class GulirMaks extends ScrollView {
        private final int maks;

        GulirMaks(Context context, int maks) {
            super(context);
            this.maks = maks;
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            int sisa = MeasureSpec.getMode(heightMeasureSpec) == MeasureSpec.UNSPECIFIED
                    ? maks : Math.min(maks, MeasureSpec.getSize(heightMeasureSpec));
            super.onMeasure(widthMeasureSpec, MeasureSpec.makeMeasureSpec(sisa, MeasureSpec.AT_MOST));
        }
    }

    private int dp(float nilai) {
        return (int) Theme.dp(context, nilai);
    }
}
