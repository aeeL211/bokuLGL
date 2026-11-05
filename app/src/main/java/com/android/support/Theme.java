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
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.style.StyleSpan;
import android.util.Log;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
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
    public static final int OUTLINE = 0xFF271100;
    public static final int STROKE_KONTROL = 0xFFB28950;
    public static final int SWITCH_ON_TRACK = 0xFFF8B551;
    public static final int SWITCH_ON_KNOB = 0xFFFFF799;
    public static final int SWITCH_OFF_TRACK = 0xFFB28950;
    public static final int SWITCH_OFF_KNOB = 0xFFCEA972;
    public static final int TAN = 0xFFC9A56F;
    public static final int TRACK = 0xFF936832;
    public static final int ACTIVE_BORDER = 0xFFF6F6F6;

    // Turunan palet yang dipakai berulang.
    public static final int FOOTER = HEADER;
    public static final int MUTED = ICON_BOX;
    public static final int ACCENT = TITLE_TEXT;

    // Sudut, dalam dp. Semua drawable mengambil nilai dari sini.
    public static final float RADIUS_PANEL = 2f;
    public static final float RADIUS_ROW = 3f;
    public static final float RADIUS_IKON = 3f;
    public static final float RADIUS_THUMB = 2f;
    public static final float RADIUS_KNOB = 3f;

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
        return kotak(OUTLINE, RADIUS_PANEL, 0f, OUTLINE, context);
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

    public static Drawable sectionHeader(Context context) {
        return kotak(HEADER, RADIUS_PANEL, TEBAL_PANEL, OUTLINE, context);
    }

    public static Drawable sectionBody(Context context) {
        return kotak(BODY, RADIUS_PANEL, TEBAL_PANEL, OUTLINE, context);
    }

    public static Drawable baris(Context context) {
        return kotak(ROW, RADIUS_ROW, TEBAL_KONTROL, STROKE_KONTROL, context);
    }

    public static Drawable kartu(Context context) {
        return kotak(BODY, RADIUS_ROW, TEBAL_KONTROL, STROKE_KONTROL, context);
    }

    /** Kartu pemain yang sedang dipilih memakai garis fokus putih. */
    public static Drawable kartuTerpilih(Context context) {
        return kotak(BODY, RADIUS_ROW, TEBAL_FOKUS, ACTIVE_BORDER, context);
    }

    public static Drawable tombol(Context context) {
        return kotak(ICON_BOX, RADIUS_PANEL, TEBAL_KONTROL, STROKE_KONTROL, context);
    }

    public static Drawable tombolAktif(Context context) {
        return kotak(ICON_BOX, RADIUS_PANEL, TEBAL_FOKUS, ACTIVE_BORDER, context);
    }

    public static Drawable tombolMati(Context context) {
        return kotak(TRACK, RADIUS_PANEL, TEBAL_KONTROL, STROKE_KONTROL, context);
    }

    public static Drawable tombolKecil(Context context, int warnaGaris) {
        return kotak(ICON_BOX, RADIUS_PANEL, TEBAL_KONTROL, warnaGaris, context);
    }

    public static Drawable lencana(Context context, int warna) {
        return kotak(warna, RADIUS_PANEL, 1f, OUTLINE, context);
    }

    public static Drawable kotakIkon(Context context) {
        return kotak(ICON_BOX, RADIUS_IKON, 1f, OUTLINE, context);
    }

    public static Drawable kotakTutup(Context context) {
        return kotak(OUTLINE, RADIUS_PANEL, 2f, ACTIVE_BORDER, context);
    }

    /**
     * Track tipis #936832 dengan isi gelap #81521D. Isi memakai warna kotak ikon
     * supaya posisi slider tetap terbaca di atas baris #DCB983.
     */
    public static Drawable seekbarProgress(Context context) {
        GradientDrawable track = kotak(TRACK, RADIUS_THUMB, 0f, TRACK, context);
        GradientDrawable isi = kotak(ICON_BOX, RADIUS_THUMB, 0f, ICON_BOX, context);
        ClipDrawable clip = new ClipDrawable(isi, Gravity.LEFT, ClipDrawable.HORIZONTAL);

        LayerDrawable layers = new LayerDrawable(new Drawable[]{track, clip});
        int inset = (int) dp(context, 12f);
        layers.setLayerInset(0, 0, inset, 0, inset);
        layers.setLayerInset(1, 0, inset, 0, inset);
        return layers;
    }

    /** Thumb persegi panjang tegak, bukan bulatan bawaan Android. */
    public static Drawable seekbarThumb(Context context) {
        GradientDrawable drawable = kotak(TITLE_TEXT, RADIUS_THUMB, 1f, OUTLINE, context);
        drawable.setSize((int) dp(context, 12f), (int) dp(context, 26f));
        return drawable;
    }

    /** Garis bergerigi tipis pemisah header dan body. */
    public static class Zigzag extends View {
        private final Paint cat = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Path path = new Path();
        private final Context ctx;

        public Zigzag(Context context) {
            super(context);
            ctx = context;
            cat.setColor(OUTLINE);
            cat.setStyle(Paint.Style.FILL);
        }

        @Override
        protected void onDraw(Canvas canvas) {
            float lebar = getWidth();
            float tinggi = getHeight();
            float langkah = dp(ctx, 6f);
            path.reset();
            for (float x = 0; x < lebar + langkah; x += langkah) {
                path.moveTo(x, tinggi);
                path.lineTo(x + langkah / 2f, 0);
                path.lineTo(x + langkah, tinggi);
            }
            canvas.drawPath(path, cat);
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

        private final Paint cat = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Context ctx;
        private final int warnaKotak;
        private final int warnaGaris;
        private final int warnaGlyph;
        private final float tebalGaris;
        private final int ukuranDp;
        private boolean pakaiKotak = true;
        private int jenis;

        public Ikon(Context context, int jenis) {
            this(context, jenis, ICON_BOX, 0, 0f, TITLE_TEXT, 32);
        }

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
            float radius = dp(ctx, RADIUS_IKON);

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
