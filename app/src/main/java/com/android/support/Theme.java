package com.android.support;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.Typeface;
import android.graphics.drawable.ClipDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.InsetDrawable;
import android.graphics.drawable.LayerDrawable;
import android.graphics.drawable.StateListDrawable;
import android.os.Build;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.style.StyleSpan;
import android.util.Log;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.ScrollView;
import android.widget.TextView;

/**
 * Palet, radius, dan pembuat drawable untuk menu bergaya panel kayu pixel-art.
 *
 * Semua warna dan ukuran sudut dipusatkan di sini, jadi Menu.java tidak lagi
 * menulis nilai hex atau angka dp sendiri.
 */
public final class Theme {
    private Theme() {
    }

    // Palet panel kayu.
    public static final int HEADER = 0xFF996C34;
    public static final int BODY = 0xFFFACA82;
    public static final int ROW = 0xFFDCB983;
    public static final int ICON_BOX = 0xFF81521D;
    public static final int TITLE_TEXT = 0xFFEABD7B;
    public static final int TEXT = 0xFF2B1A08;
    public static final int STROKE_CONTROL = 0xFFB28950;
    public static final int SWITCH_ON_TRACK = 0xFFF8B551;
    public static final int SWITCH_ON_KNOB = 0xFFFFF799;
    public static final int SWITCH_OFF_TRACK = 0xFFB28950;
    public static final int SWITCH_OFF_KNOB = 0xFFCEA972;
    public static final int TAN = 0xFFC9A56F;
    public static final int TRACK = 0xFF936832;
    public static final int ACTIVE_BORDER = 0xFFF6F6F6;

    // Tombol sudut seperti tombol kembali di game: kotak hitam, garis putih.
    public static final int BOX_CORNER = 0xFF000000;
    public static final int BOX_CORNER_PRESS = 0xFF3A3A3A;

    // Warna saat ditekan: baris sedikit lebih gelap, tombol berisi TRACK.
    public static final int ROW_PRESS = 0xFFCDA874;
    public static final int HEADER_PRESS = 0xFF86602D;

    // Tiga nada ikon pixel-art game (diambil dari ikon CRT filter / Vignette / Light Effect).
    public static final int ICON_LIGHT = 0xFFCCAA77;
    public static final int ICON_MID = 0xFFBB8855;
    public static final int ICON_DARK = 0xFF996633;

    // Garis tombol kayu: krem terang di atas isi cokelat tua.
    public static final int BUTTON_BORDER = 0xFFDCB983;

    // Turunan palet yang dipakai berulang.
    public static final int FOOTER = HEADER;
    public static final int MUTED = ICON_BOX;
    public static final int ACCENT = TITLE_TEXT;

    // Sudut, dalam dp. Semua drawable mengambil nilai dari sini.
    public static final float RADIUS_PANEL = 3f;
    public static final float RADIUS_ROW = 5f;
    public static final float RADIUS_ICON = 5f;
    public static final float RADIUS_THUMB = 3f;
    public static final float RADIUS_KNOB = 10f;

    // Tebal garis, dalam dp.
    public static final float BOLD_PANEL = 3f;
    public static final float BOLD_CONTROL = 3f;
    public static final float BOLD_FOCUS = 3f;

    // Ukuran teks. Tidak ada bold di seluruh menu.
    public static final float TITLE_SP = 22f;
    public static final float SECTION_SP = 18f;
    public static final float ROW_SP = 16f;
    public static final float DESC_SP = 14f;

    /** Ukuran sentuh minimum setiap kontrol, dalam dp. */
    public static final float TARGET_DP = 48f;

    // Slider seperti semula: SeekBar setinggi 28dp, track 4dp (inset 12dp atas-bawah), thumb 10x24dp.
    /** Tinggi SeekBar. */
    public static final float SLIDER_HEIGHT_DP = 28f;
    private static final float SLIDER_INSET_DP = 12f;
    private static final float SLIDER_THUMB_WIDTH_DP = 10f;
    private static final float SLIDER_THUMB_HEIGHT_DP = 24f;
    private static final float ZIGZAG_JARI_DP = 3f;
    private static final float ZIGZAG_STEP_DP = 8f;

    private static final String FONT_ASSET = "fonts/itim.ttf";
    private static Typeface sFont;

    public static Typeface font(Context context) {
        if (sFont == null) {
            try {
                sFont = Typeface.createFromAsset(context.getApplicationContext().getAssets(),
                        FONT_ASSET);
            } catch (Throwable t) {
                Log.d(BKLog.TAG, "Font gagal dimuat: " + t);
                sFont = Typeface.DEFAULT;
            }
        }
        return sFont;
    }

    /**
     * Memasang font game ke semua TextView, Button, Switch, CheckBox, EditText,
     * dan turunannya. Bold dari Html.fromHtml ikut dilepas supaya tetap regular.
     */
    public static void applyFont(View view) {
        if (view == null)
            return;

        if (view instanceof TextView) {
            TextView textView = (TextView) view;
            textView.setTypeface(font(view.getContext()));
            textView.setPaintFlags(textView.getPaintFlags() & ~Paint.FAKE_BOLD_TEXT_FLAG);
            if (!(textView instanceof EditText))
                deleteBold(textView);
        }

        if (view instanceof ViewGroup) {
            ViewGroup group = (ViewGroup) view;
            for (int i = 0; i < group.getChildCount(); i++)
                applyFont(group.getChildAt(i));
        }
    }

    private static void deleteBold(TextView textView) {
        CharSequence content = textView.getText();
        if (!(content instanceof Spanned))
            return;

        // Teks tidak dibuat ulang bila memang tidak ada span tebal.
        StyleSpan[] old = ((Spanned) content).getSpans(0, content.length(), StyleSpan.class);
        boolean hasBold = false;
        for (StyleSpan span : old) {
            if ((span.getStyle() & Typeface.BOLD) != 0) {
                hasBold = true;
                break;
            }
        }
        if (!hasBold)
            return;

        SpannableStringBuilder fresh = new SpannableStringBuilder(content);
        for (StyleSpan span : fresh.getSpans(0, fresh.length(), StyleSpan.class)) {
            if ((span.getStyle() & Typeface.BOLD) != 0)
                fresh.removeSpan(span);
        }
        textView.setText(fresh);
    }

    public static TextView text(Context context, CharSequence content, float sizeSp, int color) {
        TextView textView = new TextView(context);
        textView.setText(content);
        textView.setTextSize(sizeSp);
        textView.setTextColor(color);
        textView.setTypeface(font(context));
        return textView;
    }

    public static float dp(Context context, float value) {
        return TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, value,
                context.getResources().getDisplayMetrics());
    }

    public static String colorHex(int color) {
        return String.format("#%06X", 0xFFFFFF & color);
    }

    private static final double GAMMA = 2.2;

    /**
     * Campuran dua warna ARGB tanpa boxing, aman dipanggil tiap frame. Hasilnya
     * sama dengan ArgbEvaluator bawaan Android: RGB dicampur di ruang linear
     * (gamma 2.2), alpha dicampur biasa. Dicampur langsung di sRGB akan
     * membuat warna di tengah animasi tampak sedikit lebih gelap.
     */
    public static int mix(int from, int to, float f) {
        if (f <= 0f)
            return from;
        if (f >= 1f)
            return to;
        int a0 = from >>> 24;
        int a = Math.round(a0 + ((to >>> 24) - a0) * f);
        int r = kanal((from >> 16) & 0xFF, (to >> 16) & 0xFF, f);
        int g = kanal((from >> 8) & 0xFF, (to >> 8) & 0xFF, f);
        int b = kanal(from & 0xFF, to & 0xFF, f);
        return (a << 24) | (r << 16) | (g << 8) | b;
    }

    private static int kanal(int from, int to, float f) {
        double d = Math.pow(from / 255.0, GAMMA);
        double k = Math.pow(to / 255.0, GAMMA);
        double v = d + (k - d) * f;
        return (int) Math.round(Math.pow(v, 1.0 / GAMMA) * 255.0);
    }

    private static GradientDrawable box(int color, float radiusDp, float boldDp,
                                           int lineColor, Context context) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setColor(color);
        drawable.setCornerRadius(dp(context, radiusDp));
        if (boldDp > 0)
            drawable.setStroke(Math.max(1, (int) dp(context, boldDp)), lineColor);
        return drawable;
    }

    /** Drawable dengan satu tampilan biasa dan satu tampilan saat ditekan. */
    private static Drawable press(Drawable biasa, Drawable pressed) {
        StateListDrawable drawable = new StateListDrawable();
        drawable.addState(new int[]{android.R.attr.state_pressed}, pressed);
        drawable.addState(new int[]{}, biasa);
        return drawable;
    }

    /** Bingkai luar panel: isi gelap 3dp, jadi padding view membentuk outline. */
    public static Drawable panel(Context context) {
        return box(ICON_BOX, RADIUS_PANEL, 0f, ICON_BOX, context);
    }

    public static Drawable header(Context context) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setColor(HEADER);
        float r = dp(context, RADIUS_PANEL);
        drawable.setCornerRadii(new float[]{r, r, r, r, 0f, 0f, 0f, 0f});
        return drawable;
    }

    public static Drawable footer(Context context) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setColor(FOOTER);
        float r = dp(context, RADIUS_PANEL);
        drawable.setCornerRadii(new float[]{0f, 0f, 0f, 0f, r, r, r, r});
        return drawable;
    }

    /** Kepala section: pita cokelat polos, tanpa garis tebal. */
    public static Drawable sectionHeader(Context context) {
        return box(HEADER, RADIUS_ROW, 0f, HEADER, context);
    }

    /** Kepala section yang bisa diketuk: sedikit lebih gelap saat ditekan. */
    public static Drawable sectionHeaderPress(Context context) {
        return press(sectionHeader(context),
                box(HEADER_PRESS, RADIUS_ROW, 0f, HEADER_PRESS, context));
    }

    /** Isi section transparan: baris di dalamnya langsung duduk di atas BODY. */
    public static Drawable sectionBody(Context context) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setColor(0x00000000);
        return drawable;
    }

    /** Baris kontrol game: datar #DCB983 tanpa garis, sudut melengkung ±5dp. */
    public static Drawable row(Context context) {
        return box(ROW, RADIUS_ROW, 0f, ROW, context);
    }

    /** Tombol utama dialog: seperti tombol aktif (garis putih), dengan umpan balik tekan. */
    public static Drawable mainButton(Context context) {
        return press(activeButton(context),
                box(TRACK, RADIUS_PANEL, BOLD_FOCUS, ACTIVE_BORDER, context));
    }

    /** Kolom isian dialog: baris ROW dengan garis tipis, bukan garis bawah bawaan. */
    public static Drawable dialogInput(Context context) {
        return box(ROW, RADIUS_ROW, 1f, STROKE_CONTROL, context);
    }

    /** Kartu yang bisa diketuk: sedikit lebih gelap saat ditekan. */
    public static Drawable card(Context context) {
        return press(box(ROW, RADIUS_ROW, 0f, ROW, context),
                box(ROW_PRESS, RADIUS_ROW, 0f, ROW_PRESS, context));
    }

    /** Kartu pemain yang sedang dipilih memakai garis fokus putih. */
    public static Drawable cardSelected(Context context) {
        return press(box(ROW, RADIUS_ROW, BOLD_FOCUS, ACTIVE_BORDER, context),
                box(ROW_PRESS, RADIUS_ROW, BOLD_FOCUS, ACTIVE_BORDER, context));
    }

    /** Tombol kayu: isi cokelat tua dengan garis krem 3dp, sudut kecil. */
    public static Drawable button(Context context) {
        return box(ICON_BOX, RADIUS_PANEL, BOLD_CONTROL, BUTTON_BORDER, context);
    }

    public static Drawable activeButton(Context context) {
        return box(ICON_BOX, RADIUS_PANEL, BOLD_FOCUS, ACTIVE_BORDER, context);
    }

    public static Drawable offButton(Context context) {
        return box(TRACK, RADIUS_PANEL, BOLD_CONTROL, STROKE_CONTROL, context);
    }

    /** Tombol kecil: isi berubah menjadi TRACK saat ditekan supaya terasa responsif. */
    public static Drawable smallButton(Context context) {
        return smallButton(context, 0f);
    }

    /**
     * Tombol kecil dengan ruang sentuh lebih tinggi dari gambarnya. View-nya
     * setinggi 48dp, tetapi kotaknya digambar menyusut {@code insetVertDp} di
     * atas dan bawah, jadi tampilan tetap sama dan jari tetap kena.
     */
    public static Drawable smallButton(Context context, float insetVertDp) {
        Drawable drawable = press(
                box(ICON_BOX, RADIUS_PANEL, 2f, BUTTON_BORDER, context),
                box(TRACK, RADIUS_PANEL, 2f, BUTTON_BORDER, context));
        if (insetVertDp <= 0f)
            return drawable;
        int v = (int) dp(context, insetVertDp);
        return new InsetDrawable(drawable, 0, v, 0, v);
    }

    /** Tombol kayu penuh (footer) dengan umpan balik tekan. */
    public static Drawable pressButton(Context context) {
        return press(button(context),
                box(TRACK, RADIUS_PANEL, BOLD_CONTROL, BUTTON_BORDER, context));
    }

    /**
     * Lapisan tipis untuk Button di dalam kotak tombol yang sudah punya garis:
     * transparan biasanya, terang sedikit saat ditekan.
     */
    public static Drawable pressOverlay(Context context) {
        return press(box(0x00000000, RADIUS_PANEL, 0f, 0x00000000, context),
                box(0x26FFFFFF, RADIUS_PANEL, 0f, 0x26FFFFFF, context));
    }

    /** Baris yang bisa diketuk: sedikit lebih gelap saat ditekan. */
    public static Drawable rowPress(Context context) {
        return press(row(context),
                box(ROW_PRESS, RADIUS_ROW, 0f, ROW_PRESS, context));
    }

    /** Scrollbar tipis berwarna TRACK, seperti penanda gulir di game. */
    public static void gayaScrollbar(Context context, ScrollView scrollView) {
        scrollView.setVerticalScrollBarEnabled(true);
        scrollView.setScrollBarSize((int) dp(context, 3f));
        if (Build.VERSION.SDK_INT >= 29)
            scrollView.setVerticalScrollbarThumbDrawable(box(TRACK, 2f, 0f, TRACK, context));
    }

    public static Drawable badge(Context context, int color) {
        return box(color, RADIUS_PANEL, 0f, color, context);
    }

    /**
     * Track tipis 4dp seperti slider game: garis #936832, bagian yang sudah
     * terisi memakai cokelat tua #81521D supaya posisi tetap terbaca di atas
     * baris #DCB983. Tinggi SeekBar 28dp dengan inset 12dp atas-bawah.
     */
    public static Drawable seekbarProgress(Context context) {
        GradientDrawable track = box(TRACK, 2f, 0f, TRACK, context);
        GradientDrawable content = box(ICON_BOX, 2f, 0f, ICON_BOX, context);
        ClipDrawable clip = new ClipDrawable(content, Gravity.LEFT, ClipDrawable.HORIZONTAL);

        LayerDrawable layers = new LayerDrawable(new Drawable[]{track, clip});
        int inset = (int) dp(context, SLIDER_INSET_DP);
        layers.setLayerInset(0, 0, inset, 0, inset);
        layers.setLayerInset(1, 0, inset, 0, inset);
        return layers;
    }

    /** Thumb persegi panjang tegak #936832 seperti game, tanpa garis hitam. */
    public static Drawable seekbarThumb(Context context) {
        GradientDrawable drawable = box(TRACK, RADIUS_THUMB, 0f, TRACK, context);
        drawable.setSize((int) dp(context, SLIDER_THUMB_WIDTH_DP),
                (int) dp(context, SLIDER_THUMB_HEIGHT_DP));
        return drawable;
    }

    /**
     * Tepi bergelombang pemisah header/body, mirip tepi bergerigi di game.
     * Setengah lingkaran berwarna {@code warnaBump} digambar berderet di atas
     * latar view. Bila {@code menggantung} benar, gelombang menggantung dari
     * tepi atas (di bawah header); bila salah, naik dari tepi bawah (di atas
     * footer).
     */
    public static class Zigzag extends View {
        private final Paint cat = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final float jariDp;
        private final float step;
        private final boolean menggantung;

        public Zigzag(Context context, int bumpColor, boolean menggantung) {
            super(context);
            this.menggantung = menggantung;
            this.jariDp = dp(context, ZIGZAG_JARI_DP);
            this.step = dp(context, ZIGZAG_STEP_DP);
            cat.setStyle(Paint.Style.FILL);
            cat.setColor(bumpColor);
        }

        @Override
        protected void onDraw(Canvas canvas) {
            float width = getWidth();
            float height = getHeight();
            float jari = Math.min(height, jariDp);
            float cy = menggantung ? 0f : height;
            for (float x = step / 2f; x < width + step; x += step)
                canvas.drawCircle(x, cy, jari, cat);
        }
    }

    /**
     * Panah lipatan kecil (kanan atau bawah). Ikon fitur memakai IkonFitur
     * karena bentuknya grid 16x16. Path dibuat sekali saat ukuran berubah.
     */
    public static class Icon extends View {
        public static final int ARROW_RIGHT = 2;
        public static final int ARROW_DOWN = 3;

        private final Paint cat = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Path path = new Path();
        private final int sizePx;
        private int jenis;

        public static Icon polos(Context context, int jenis, int glyphColor, int sizeDp) {
            return new Icon(context, jenis, glyphColor, sizeDp);
        }

        private Icon(Context context, int jenis, int glyphColor, int sizeDp) {
            super(context);
            this.jenis = jenis;
            this.sizePx = (int) dp(context, sizeDp);
            cat.setStyle(Paint.Style.FILL);
            cat.setColor(glyphColor);
        }

        public void setJenis(int jenis) {
            if (this.jenis == jenis)
                return;
            this.jenis = jenis;
            shape(getWidth(), getHeight());
            invalidate();
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            setMeasuredDimension(resolveSize(sizePx, widthMeasureSpec),
                    resolveSize(sizePx, heightMeasureSpec));
        }

        @Override
        protected void onSizeChanged(int w, int h, int oldw, int oldh) {
            super.onSizeChanged(w, h, oldw, oldh);
            shape(w, h);
        }

        private void shape(int width, int height) {
            float cx = width / 2f;
            float cy = height / 2f;
            float s = Math.min(width, height);
            path.reset();
            if (jenis == ARROW_DOWN) {
                path.moveTo(cx - s * 0.24f, cy - s * 0.14f);
                path.lineTo(cx + s * 0.24f, cy - s * 0.14f);
                path.lineTo(cx, cy + s * 0.22f);
            } else {
                path.moveTo(cx - s * 0.16f, cy - s * 0.24f);
                path.lineTo(cx + s * 0.22f, cy);
                path.lineTo(cx - s * 0.16f, cy + s * 0.24f);
            }
            path.close();
        }

        @Override
        protected void onDraw(Canvas canvas) {
            canvas.drawPath(path, cat);
        }
    }
}
