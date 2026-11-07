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
final class MenuDialog {
    private static final String TAG = Menu.TAG;
    private static final int TYPE_OVERLAY = Build.VERSION.SDK_INT >= 26 ? 2038 : 2002;

    /** Lebar maksimum sama dengan lebar menu, dengan sisa tepi layar. */
    private static final float WIDTH_MAX_DP = 340f;
    private static final float MARGIN_SCREEN_DP = 16f;
    /** Isi yang lebih tinggi dari ini digulir, supaya tombol tidak terdorong keluar layar. */
    private static final float HEIGHT_CONTENT_MAX = 0.5f;
    private static final float REDUP = 0.5f;
    private static final float TEXT_MESSAGE_SP = Theme.ROW_SP;

    /** Penerima teks yang dimasukkan pengguna. */
    interface Receiver {
        void diterima(String text);
    }

    /** Satu tombol: label dan aksi yang dijalankan sebelum dialog ditutup. */
    static final class Button {
        final String label;
        final Runnable action;

        Button(String label, Runnable action) {
            this.label = label == null ? "" : label;
            this.action = action;
        }
    }

    /** Dialog yang tampil beserta tombolnya (null bila tombol itu tidak ada). */
    static final class Result {
        final Dialog dialog;
        Button positif;
        Button neutral;
        Button negatif;

        Result(Dialog dialog) {
            this.dialog = dialog;
        }
    }

    private final Context context;
    private boolean overlay;
    private Dialog opened;

    MenuDialog(Context context) {
        this.context = context;
    }

    /** Dialog harus memakai jendela overlay bila menu berjalan sebagai Service. */
    void setOverlay(boolean overlay) {
        this.overlay = overlay;
    }

    /** Menutup dialog yang masih terbuka. Aman dipanggil berulang. */
    void close() {
        Dialog dialog = opened;
        opened = null;
        if (dialog != null && dialog.isShowing()) {
            try {
                dialog.dismiss();
            } catch (Throwable t) {
                Log.d(TAG, "Dialog gagal ditutup: " + t);
            }
        }
    }

    /** Dialog penjelasan yang dibuka dengan menekan lama sebuah fitur. */
    void explanation(int featNum, String featName) {
        display(Html.fromHtml(featName), Explanation.forTarget(featNum, featName), null, null,
                new Button("Mengerti", null), null, null, true);
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
    void input(String title, String hint, String initial, boolean valueText, int maxCharacter,
               final Receiver ok, String neutral, Runnable actionNeutral) {
        final EditText isian = new EditText(context);
        isian.setTextColor(Theme.TEXT);
        isian.setTextSize(Theme.ROW_SP);
        isian.setHintTextColor(Theme.MUTED);
        isian.setBackground(Theme.dialogInput(context));
        isian.setMinHeight(dp(Theme.TARGET_DP));
        isian.setPadding(dp(12), dp(8), dp(12), dp(8));
        isian.setSingleLine(true);
        isian.setImeOptions(EditorInfo.IME_ACTION_DONE);
        if (valueText) {
            isian.setInputType(InputType.TYPE_CLASS_NUMBER);
            isian.setKeyListener(DigitsKeyListener.getInstance("0123456789-"));
        }
        if (maxCharacter > 0)
            isian.setFilters(new InputFilter[]{new InputFilter.LengthFilter(maxCharacter)});
        if (initial != null && initial.length() > 0) {
            isian.setText(initial);
            isian.setSelection(isian.length());
        }

        LinearLayout content = new LinearLayout(context);
        content.setOrientation(LinearLayout.VERTICAL);
        if (hint != null && hint.length() > 0) {
            TextView keterangan = Theme.text(context, hint, Theme.DESC_SP, Theme.MUTED);
            keterangan.setPadding(dp(2), 0, dp(2), dp(6));
            content.addView(keterangan, new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));
        }
        content.addView(isian, new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));

        final Result[] result = new Result[1];
        isian.setOnEditorActionListener(new TextView.OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                if (actionId == EditorInfo.IME_ACTION_DONE && result[0] != null
                        && result[0].positif != null) {
                    result[0].positif.performClick();
                    return true;
                }
                return false;
            }
        });

        Button okButton = new Button("OK", new Runnable() {
            @Override
            public void run() {
                ok.diterima(isian.getText().toString());
            }
        });
        Button neutralButton = neutral != null && actionNeutral != null
                ? new Button(neutral, actionNeutral) : null;
        result[0] = display(title, null, content, isian, okButton, neutralButton,
                new Button("Batal", null), false);
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
    Result display(CharSequence title, CharSequence message, View content, EditText focus,
                    Button positif, Button neutral, Button negatif, boolean closeOutside) {
        close();

        final Dialog dialog = new Dialog(context, android.R.style.Theme_DeviceDefault_Dialog);
        dialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        Result result = new Result(dialog);

        DisplayMetrics dm = context.getResources().getDisplayMetrics();
        int width = Math.min(dp(WIDTH_MAX_DP), dm.widthPixels - 2 * dp(MARGIN_SCREEN_DP));

        // Bingkai luar: isi gelap 3dp, padding view membentuk garis tepi.
        LinearLayout root = new LinearLayout(context);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackground(Theme.panel(context));
        root.setPadding(dp(3), dp(3), dp(3), dp(3));

        TextView kepala = Theme.text(context, title == null ? "" : title, Theme.SECTION_SP,
                Theme.TITLE_TEXT);
        kepala.setGravity(Gravity.CENTER);
        kepala.setMaxLines(2);
        kepala.setEllipsize(TextUtils.TruncateAt.END);
        kepala.setMinHeight(dp(Theme.TARGET_DP));
        kepala.setPadding(dp(14), dp(8), dp(14), dp(8));
        kepala.setBackground(Theme.header(context));
        root.addView(kepala, new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));

        Theme.Zigzag zigTop = new Theme.Zigzag(context, Theme.HEADER, true);
        zigTop.setBackgroundColor(Theme.BODY);
        root.addView(zigTop, new LinearLayout.LayoutParams(MATCH_PARENT, dp(5)));

        LinearLayout body = new LinearLayout(context);
        body.setOrientation(LinearLayout.VERTICAL);
        body.setPadding(dp(14), dp(8), dp(14), dp(12));
        if (message != null && message.length() > 0) {
            TextView text = Theme.text(context, message, TEXT_MESSAGE_SP, Theme.TEXT);
            text.setLineSpacing(dp(2), 1f);
            body.addView(text, new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));
        }
        if (content != null)
            body.addView(content, new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));

        ScrollMax scroll = new ScrollMax(context, (int) (dm.heightPixels * HEIGHT_CONTENT_MAX));
        scroll.setBackgroundColor(Theme.BODY);
        Theme.gayaScrollbar(context, scroll);
        scroll.addView(body, new ViewGroup.LayoutParams(MATCH_PARENT, WRAP_CONTENT));
        root.addView(scroll, new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));

        Theme.Zigzag zigBottom = new Theme.Zigzag(context, Theme.FOOTER, false);
        zigBottom.setBackgroundColor(Theme.BODY);
        root.addView(zigBottom, new LinearLayout.LayoutParams(MATCH_PARENT, dp(5)));

        // Urutan kiri ke kanan: negatif, netral, positif (tombol utama di kanan).
        LinearLayout kaki = new LinearLayout(context);
        kaki.setOrientation(LinearLayout.HORIZONTAL);
        kaki.setBackground(Theme.footer(context));
        kaki.setPadding(dp(8), dp(6), dp(8), dp(6));
        result.negatif = addButton(kaki, dialog, negatif, false);
        result.neutral = addButton(kaki, dialog, neutral, false);
        result.positif = addButton(kaki, dialog, positif, true);
        root.addView(kaki, new LinearLayout.LayoutParams(MATCH_PARENT, WRAP_CONTENT));

        Theme.applyFont(root);
        dialog.setContentView(root, new ViewGroup.LayoutParams(width, WRAP_CONTENT));
        dialog.setCanceledOnTouchOutside(closeOutside);

        Window window = dialog.getWindow();
        if (window != null) {
            window.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
            window.setLayout(width, WRAP_CONTENT);
            window.setGravity(Gravity.CENTER);
            window.addFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);
            window.setDimAmount(REDUP);
            if (overlay)
                window.setType(TYPE_OVERLAY);
            if (focus != null)
                window.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_VISIBLE
                        | WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
        }

        try {
            dialog.show();
        } catch (Throwable t) {
            Log.d(TAG, "Dialog gagal ditampilkan: " + t);
            return null;
        }
        opened = dialog;
        Transition.enter(root);
        if (focus != null)
            munculkanKeyboard(focus);
        return result;
    }

    private Button addButton(LinearLayout kaki, final Dialog dialog, final Button button,
                                boolean main) {
        if (button == null)
            return null;

        Button button = new Button(context);
        button.setText(button.label);
        button.setTextColor(Theme.TITLE_TEXT);
        button.setTextSize(Theme.ROW_SP);
        button.setTypeface(Theme.font(context));
        button.setAllCaps(false);
        button.setSingleLine(true);
        button.setEllipsize(TextUtils.TruncateAt.END);
        button.setGravity(Gravity.CENTER);
        button.setBackground(main ? Theme.mainButton(context) : Theme.pressButton(context));
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
                    if (button.action != null)
                        button.action.run();
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
    private static final class ScrollMax extends ScrollView {
        private final int max;

        ScrollMax(Context context, int max) {
            super(context);
            this.max = max;
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            int remaining = MeasureSpec.getMode(heightMeasureSpec) == MeasureSpec.UNSPECIFIED
                    ? max : Math.min(max, MeasureSpec.getSize(heightMeasureSpec));
            super.onMeasure(widthMeasureSpec, MeasureSpec.makeMeasureSpec(remaining, MeasureSpec.AT_MOST));
        }
    }

    private int dp(float value) {
        return (int) Theme.dp(context, value);
    }
}
