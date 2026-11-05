package com.android.support;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.Typeface;
import android.graphics.drawable.ClipDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
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
    public static final int STROKE_KONTROL = 0xFFB28950;
    public static final int SWITCH_ON_TRACK = 0xFFF8B551;
    public static final int SWITCH_ON_KNOB = 0xFFFFF799;
    public static final int SWITCH_OFF_TRACK = 0xFFB28950;
    public static final int SWITCH_OFF_KNOB = 0xFFCEA972;
    public static final int TAN = 0xFFC9A56F;
    public static final int TRACK = 0xFF936832;
    public static final int ACTIVE_BORDER = 0xFFF6F6F6;

    // Tombol sudut seperti tombol kembali di game: kotak hitam, garis putih.
    public static final int BOX_SUDUT = 0xFF000000;

    // Warna saat ditekan: baris sedikit lebih gelap, tombol berisi TRACK.
    public static final int ROW_TEKAN = 0xFFCDA874;

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
    public static final float RADIUS_IKON = 5f;
    public static final float RADIUS_THUMB = 3f;
    public static final float RADIUS_KNOB = 10f;

    // Tebal garis, dalam dp.
    public static final float TEBAL_PANEL = 3f;
    public static final float TEBAL_KONTROL = 3f;
    public static final float TEBAL_FOKUS = 3f;

    // Ukuran teks. Tidak ada bold di seluruh menu.
    public static final float TITLE_SP = 22f;
    public static final float SECTION_SP = 18f;
    public static final float ROW_SP = 16f;
    public static final float DESC_SP = 14f;

    private static final String FONT_ASSET = "fonts/itim.ttf";
    private static Typeface sFont;

    public static Typeface font(Context context) {
        if (sFont == null) {
            try {
                sFont = Typeface.createFromAsset(context.getApplicationContext().getAssets(),
                        FONT_ASSET);
            } catch (Throwable t) {
                Log.d("Mod_Menu", "Font gagal dimuat: " + t);
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
                hapusTebal(textView);
        }

        if (view instanceof ViewGroup) {
            ViewGroup group = (ViewGroup) view;
            for (int i = 0; i < group.getChildCount(); i++)
                applyFont(group.getChildAt(i));
        }
    }

    private static void hapusTebal(TextView textView) {
        CharSequence isi = textView.getText();
        if (!(isi instanceof Spanned))
            return;

        SpannableStringBuilder baru = new SpannableStringBuilder(isi);
        StyleSpan[] spans = baru.getSpans(0, baru.length(), StyleSpan.class);
        for (StyleSpan span : spans) {
            if ((span.getStyle() & Typeface.BOLD) != 0)
                baru.removeSpan(span);
        }
        textView.setText(baru);
    }

    public static TextView teks(Context context, CharSequence isi, float ukuranSp, int warna) {
        TextView textView = new TextView(context);
        textView.setText(isi);
        textView.setTextSize(ukuranSp);
        textView.setTextColor(warna);
        textView.setTypeface(font(context));
        return textView;
    }

    public static float dp(Context context, float nilai) {
        return TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, nilai,
                context.getResources().getDisplayMetrics());
    }

    public static String warnaHex(int warna) {
        return String.format("#%06X", 0xFFFFFF & warna);
    }

    private static GradientDrawable kotak(int warna, float radiusDp, float tebalDp,
                                           int warnaGaris, Context context) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setColor(warna);
        drawable.setCornerRadius(dp(context, radiusDp));
        if (tebalDp > 0)
            drawable.setStroke(Math.max(1, (int) dp(context, tebalDp)), warnaGaris);
        return drawable;
    }

    /** Bingkai luar panel: isi gelap 3dp, jadi padding view membentuk outline. */
    public static Drawable panel(Context context) {
        return kotak(ICON_BOX, RADIUS_PANEL, 0f, ICON_BOX, context);
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
        return kotak(HEADER, RADIUS_ROW, 0f, HEADER, context);
    }

    /** Isi section transparan: baris di dalamnya langsung duduk di atas BODY. */
    public static Drawable sectionBody(Context context) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setColor(0x00000000);
        return drawable;
    }

    /** Baris kontrol game: datar #DCB983 tanpa garis, sudut melengkung ±5dp. */
    public static Drawable baris(Context context) {
        return kotak(ROW, RADIUS_ROW, 0f, ROW, context);
    }

    public static Drawable kartu(Context context) {
        return kotak(ROW, RADIUS_ROW, 0f, ROW, context);
    }

    /** Kartu pemain yang sedang dipilih memakai garis fokus putih. */
    public static Drawable kartuTerpilih(Context context) {
        return kotak(ROW, RADIUS_ROW, TEBAL_FOKUS, ACTIVE_BORDER, context);
    }

    /** Tombol kayu: isi cokelat tua dengan garis krem 3dp, sudut kecil. */
    public static Drawable tombol(Context context) {
        return kotak(ICON_BOX, RADIUS_PANEL, TEBAL_KONTROL, BUTTON_BORDER, context);
    }

    public static Drawable tombolAktif(Context context) {
        return kotak(ICON_BOX, RADIUS_PANEL, TEBAL_FOKUS, ACTIVE_BORDER, context);
    }

    public static Drawable tombolMati(Context context) {
        return kotak(TRACK, RADIUS_PANEL, TEBAL_KONTROL, STROKE_KONTROL, context);
    }

    /** Tombol kecil: isi berubah menjadi TRACK saat ditekan supaya terasa responsif. */
    public static Drawable tombolKecil(Context context) {
        StateListDrawable drawable = new StateListDrawable();
        drawable.addState(new int[]{android.R.attr.state_pressed},
                kotak(TRACK, RADIUS_PANEL, 2f, BUTTON_BORDER, context));
        drawable.addState(new int[]{},
                kotak(ICON_BOX, RADIUS_PANEL, 2f, BUTTON_BORDER, context));
        return drawable;
    }

    /** Tombol kayu penuh (footer) dengan umpan balik tekan. */
    public static Drawable tombolTekan(Context context) {
        StateListDrawable drawable = new StateListDrawable();
        drawable.addState(new int[]{android.R.attr.state_pressed},
                kotak(TRACK, RADIUS_PANEL, TEBAL_KONTROL, BUTTON_BORDER, context));
        drawable.addState(new int[]{}, tombol(context));
        return drawable;
    }

    /**
     * Lapisan tipis untuk Button di dalam kotak tombol yang sudah punya garis:
     * transparan biasanya, terang sedikit saat ditekan.
     */
    public static Drawable tekanOverlay(Context context) {
        StateListDrawable drawable = new StateListDrawable();
        drawable.addState(new int[]{android.R.attr.state_pressed},
                kotak(0x26FFFFFF, RADIUS_PANEL, 0f, 0x26FFFFFF, context));
        drawable.addState(new int[]{}, kotak(0x00000000, RADIUS_PANEL, 0f, 0x00000000, context));
        return drawable;
    }

    /** Baris yang bisa diketuk: sedikit lebih gelap saat ditekan. */
    public static Drawable barisTekan(Context context) {
        StateListDrawable drawable = new StateListDrawable();
        drawable.addState(new int[]{android.R.attr.state_pressed},
                kotak(ROW_TEKAN, RADIUS_ROW, 0f, ROW_TEKAN, context));
        drawable.addState(new int[]{}, baris(context));
        return drawable;
    }

    /** Scrollbar tipis berwarna TRACK, seperti penanda gulir di game. */
    public static void gayaScrollbar(Context context, ScrollView scrollView) {
        scrollView.setVerticalScrollBarEnabled(true);
        scrollView.setScrollBarSize((int) dp(context, 3f));
        if (Build.VERSION.SDK_INT >= 29)
            scrollView.setVerticalScrollbarThumbDrawable(kotak(TRACK, 2f, 0f, TRACK, context));
    }

    public static Drawable lencana(Context context, int warna) {
        return kotak(warna, RADIUS_PANEL, 0f, warna, context);
    }

    /**
     * Track tipis 4dp seperti slider game: garis #936832, bagian yang sudah
     * terisi memakai cokelat tua #81521D supaya posisi tetap terbaca di atas
     * baris #DCB983. Tinggi SeekBar 28dp dengan inset 12dp atas-bawah.
     */
    public static Drawable seekbarProgress(Context context) {
        GradientDrawable track = kotak(TRACK, 2f, 0f, TRACK, context);
        GradientDrawable isi = kotak(ICON_BOX, 2f, 0f, ICON_BOX, context);
        ClipDrawable clip = new ClipDrawable(isi, Gravity.LEFT, ClipDrawable.HORIZONTAL);

        LayerDrawable layers = new LayerDrawable(new Drawable[]{track, clip});
        int inset = (int) dp(context, 12f);
        layers.setLayerInset(0, 0, inset, 0, inset);
        layers.setLayerInset(1, 0, inset, 0, inset);
        return layers;
    }

    /** Thumb persegi panjang tegak #936832 seperti game, tanpa garis hitam. */
    public static Drawable seekbarThumb(Context context) {
        GradientDrawable drawable = kotak(TRACK, RADIUS_THUMB, 0f, TRACK, context);
        drawable.setSize((int) dp(context, 10f), (int) dp(context, 24f));
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
        private final Context ctx;
        private final int warnaBump;
        private final boolean menggantung;

        public Zigzag(Context context, int warnaBump, boolean menggantung) {
            super(context);
            ctx = context;
            this.warnaBump = warnaBump;
            this.menggantung = menggantung;
            cat.setStyle(Paint.Style.FILL);
            cat.setColor(warnaBump);
        }

        @Override
        protected void onDraw(Canvas canvas) {
            float lebar = getWidth();
            float tinggi = getHeight();
            float jari = Math.min(tinggi, dp(ctx, 3f));
            float langkah = dp(ctx, 8f);
            float cy = menggantung ? 0f : tinggi;
            for (float x = langkah / 2f; x < lebar + langkah; x += langkah)
                canvas.drawCircle(x, cy, jari, cat);
        }
    }

    /**
     * Ikon struktural kecil: roda gigi setelan, tanda tutup, dan panah lipatan.
     * Ikon fitur memakai IkonFitur karena bentuknya grid 16x16.
     */
    public static class Ikon extends View {
        public static final int SETELAN = 0;
        public static final int TUTUP = 1;
        public static final int PANAH_KANAN = 2;
        public static final int PANAH_BAWAH = 3;
        public static final int MINIMAL = 4;

        private final Paint cat = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Context ctx;
        private final int warnaKotak;
        private final int warnaGaris;
        private final int warnaGlyph;
        private final float tebalGaris;
        private final int ukuranDp;
        private boolean pakaiKotak = true;
        private float radiusDp = RADIUS_IKON;
        private int jenis;

        public Ikon(Context context, int jenis, int warnaKotak, int warnaGaris,
                    float tebalGarisDp, int warnaGlyph, int ukuranDp) {
            super(context);
            ctx = context;
            this.jenis = jenis;
            this.warnaKotak = warnaKotak;
            this.warnaGaris = warnaGaris;
            this.tebalGaris = tebalGarisDp;
            this.warnaGlyph = warnaGlyph;
            this.ukuranDp = ukuranDp;
        }

        public static Ikon polos(Context context, int jenis, int warnaGlyph, int ukuranDp) {
            Ikon ikon = new Ikon(context, jenis, 0, 0, 0f, warnaGlyph, ukuranDp);
            ikon.pakaiKotak = false;
            return ikon;
        }

        /** Sudut kotak yang lebih tegas, untuk tombol sudut bergaya game. */
        public Ikon siku(float radiusDp) {
            this.radiusDp = radiusDp;
            return this;
        }

        public void setJenis(int jenis) {
            this.jenis = jenis;
            invalidate();
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            int sisi = (int) dp(ctx, ukuranDp);
            setMeasuredDimension(resolveSize(sisi, widthMeasureSpec),
                    resolveSize(sisi, heightMeasureSpec));
        }

        @Override
        protected void onDraw(Canvas canvas) {
            float lebar = getWidth();
            float tinggi = getHeight();
            float radius = dp(ctx, radiusDp);

            if (pakaiKotak) {
                cat.setStyle(Paint.Style.FILL);
                cat.setColor(warnaKotak);
                canvas.drawRoundRect(0, 0, lebar, tinggi, radius, radius, cat);
                if (tebalGaris > 0) {
                    float masuk = dp(ctx, tebalGaris) / 2f;
                    cat.setStyle(Paint.Style.STROKE);
                    cat.setStrokeWidth(dp(ctx, tebalGaris));
                    cat.setColor(warnaGaris);
                    canvas.drawRoundRect(masuk, masuk, lebar - masuk, tinggi - masuk,
                            radius, radius, cat);
                }
            }

            cat.setColor(warnaGlyph);
            gambarGlyph(canvas, lebar, tinggi);
        }

        private void gambarGlyph(Canvas canvas, float lebar, float tinggi) {
            float cx = lebar / 2f;
            float cy = tinggi / 2f;
            float s = Math.min(lebar, tinggi);
            float kuas = dp(ctx, 2f);

            switch (jenis) {
                case SETELAN: {
                    cat.setStyle(Paint.Style.STROKE);
                    cat.setStrokeWidth(kuas);
                    float panjang = s * 0.56f;
                    for (int i = -1; i <= 1; i++) {
                        float y = cy + i * s * 0.2f;
                        canvas.drawLine(cx - panjang / 2f, y, cx + panjang / 2f, y, cat);
                    }
                    cat.setStyle(Paint.Style.FILL);
                    float knob = dp(ctx, 4f);
                    canvas.drawRect(cx - s * 0.18f - knob / 2f, cy - s * 0.2f - knob / 2f,
                            cx - s * 0.18f + knob / 2f, cy - s * 0.2f + knob / 2f, cat);
                    canvas.drawRect(cx + s * 0.1f - knob / 2f, cy + s * 0.2f - knob / 2f,
                            cx + s * 0.1f + knob / 2f, cy + s * 0.2f + knob / 2f, cat);
                    break;
                }
                case PANAH_BAWAH:
                case PANAH_KANAN: {
                    Path path = new Path();
                    if (jenis == PANAH_BAWAH) {
                        path.moveTo(cx - s * 0.24f, cy - s * 0.14f);
                        path.lineTo(cx + s * 0.24f, cy - s * 0.14f);
                        path.lineTo(cx, cy + s * 0.22f);
                    } else {
                        path.moveTo(cx - s * 0.16f, cy - s * 0.24f);
                        path.lineTo(cx + s * 0.22f, cy);
                        path.lineTo(cx - s * 0.16f, cy + s * 0.24f);
                    }
                    path.close();
                    cat.setStyle(Paint.Style.FILL);
                    canvas.drawPath(path, cat);
                    break;
                }
                case MINIMAL: {
                    // Garis tebal di bawah tengah, seperti tombol minimize jendela.
                    cat.setStyle(Paint.Style.FILL);
                    float panjang = s * 0.46f;
                    float tebal = dp(ctx, 3f);
                    float y = cy + s * 0.14f;
                    canvas.drawRect(cx - panjang / 2f, y - tebal / 2f,
                            cx + panjang / 2f, y + tebal / 2f, cat);
                    break;
                }
                case TUTUP: {
                    cat.setStyle(Paint.Style.STROKE);
                    cat.setStrokeWidth(kuas);
                    float ujung = s * 0.18f;
                    canvas.drawLine(cx - ujung, cy - ujung, cx + ujung, cy + ujung, cat);
                    canvas.drawLine(cx + ujung, cy - ujung, cx - ujung, cy + ujung, cat);
                    break;
                }
                default:
                    break;
            }
        }
    }
}
