package com.android.support;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.RectF;
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
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.TextView;

/**
 * Palet dan pembuat drawable untuk menu bergaya panel kayu pixel-art.
 *
 * Semua warna diambil dari daftar yang disepakati, jadi tidak ada nilai hex
 * yang ditulis ulang di Menu.java. Drawable dibuat di sini supaya Menu.java
 * hanya mengurus susunan view.
 */
public final class Theme {
    private Theme() {
    }

    // Palet panel kayu.
    public static final int HEADER = 0xFF926732;
    public static final int BODY = 0xFFEEC07C;
    public static final int FOOTER = 0xFFEDAD4D;
    public static final int ROW = 0xFFC9A56F;
    public static final int ICON_BOX = 0xFF7D4F1C;
    public static final int TRACK = 0xFF936832;
    public static final int TITLE_TEXT = 0xFFF5D9A0;
    public static final int TEXT = 0xFF2B1A08;
    public static final int MUTED = 0xFF936832;
    public static final int OUTLINE = 0xFF271100;
    public static final int ACTIVE_BORDER = 0xFFF6F6F6;
    public static final int ACCENT = 0xFFE9714F;
    public static final int VINE = 0xFF2E8B2E;

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
        return kotak(OUTLINE, 2f, 0f, OUTLINE, context);
    }

    public static Drawable header(Context context) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setColor(HEADER);
        float r = dp(context, 2f);
        drawable.setCornerRadii(new float[]{r, r, r, r, 0f, 0f, 0f, 0f});
        return drawable;
    }

    public static Drawable footer(Context context) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setColor(FOOTER);
        float r = dp(context, 2f);
        drawable.setCornerRadii(new float[]{0f, 0f, 0f, 0f, r, r, r, r});
        return drawable;
    }

    public static Drawable sectionHeader(Context context) {
        return kotak(HEADER, 2f, 2f, OUTLINE, context);
    }

    public static Drawable sectionBody(Context context) {
        return kotak(ROW, 2f, 2f, OUTLINE, context);
    }

    public static Drawable baris(Context context) {
        return kotak(ROW, 3f, 1f, TRACK, context);
    }

    public static Drawable kartu(Context context) {
        return kotak(BODY, 3f, 2f, TRACK, context);
    }

    public static Drawable tombol(Context context) {
        return kotak(ICON_BOX, 2f, 3f, ROW, context);
    }

    public static Drawable tombolAktif(Context context) {
        return kotak(ICON_BOX, 2f, 3f, ACTIVE_BORDER, context);
    }

    public static Drawable tombolMati(Context context) {
        return kotak(TRACK, 2f, 3f, ROW, context);
    }

    public static Drawable tombolKecil(Context context, int warnaGaris) {
        return kotak(ICON_BOX, 2f, 2f, warnaGaris, context);
    }

    public static Drawable lencana(Context context, int warna) {
        return kotak(warna, 2f, 1f, OUTLINE, context);
    }

    public static Drawable kotakIkon(Context context) {
        return kotak(ICON_BOX, 3f, 1f, OUTLINE, context);
    }

    public static Drawable kotakTutup(Context context) {
        return kotak(OUTLINE, 2f, 2f, ACTIVE_BORDER, context);
    }

    /**
     * Track tipis #936832 dengan isi gelap #7D4F1C. Isi memakai warna kotak ikon
     * supaya posisi slider tetap terbaca di atas baris #C9A56F.
     */
    public static Drawable seekbarProgress(Context context) {
        GradientDrawable track = kotak(TRACK, 3f, 0f, TRACK, context);
        GradientDrawable isi = kotak(ICON_BOX, 3f, 0f, ICON_BOX, context);
        ClipDrawable clip = new ClipDrawable(isi, Gravity.LEFT, ClipDrawable.HORIZONTAL);

        LayerDrawable layers = new LayerDrawable(new Drawable[]{track, clip});
        int inset = (int) dp(context, 8f);
        layers.setLayerInset(0, 0, inset, 0, inset);
        layers.setLayerInset(1, 0, inset, 0, inset);
        return layers;
    }

    /** Thumb persegi panjang tegak, bukan bulatan bawaan Android. */
    public static Drawable seekbarThumb(Context context) {
        GradientDrawable drawable = kotak(TRACK, 2f, 1f, OUTLINE, context);
        drawable.setSize((int) dp(context, 12f), (int) dp(context, 26f));
        return drawable;
    }

    /**
     * Sakelar pixel: track #C9A56F bergaris #936832, thumb #7D4F1C saat ON dan
     * #936832 saat OFF. Tetap CompoundButton, jadi listener dan Preferences
     * berjalan sama seperti Switch bawaan.
     */
    public static class PixelSwitch extends CompoundButton {
        private final Paint cat = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Context ctx;

        public PixelSwitch(Context context) {
            super(context);
            ctx = context;
            setClickable(true);
            setFocusable(true);
            setBackground(null);
            setPadding(0, 0, 0, 0);
            setButtonDrawable(null);
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            int lebar = resolveSize((int) dp(ctx, 48f), widthMeasureSpec);
            int tinggi = resolveSize((int) dp(ctx, 28f), heightMeasureSpec);
            setMeasuredDimension(lebar, tinggi);
        }

        @Override
        public void setChecked(boolean checked) {
            super.setChecked(checked);
            invalidate();
        }

        @Override
        protected void drawableStateChanged() {
            super.drawableStateChanged();
            invalidate();
        }

        @Override
        protected void onDraw(Canvas canvas) {
            float lebar = getWidth();
            float tinggi = getHeight();
            float trackTinggi = dp(ctx, 18f);
            float atas = (tinggi - trackTinggi) / 2f;
            RectF track = new RectF(0, atas, lebar, atas + trackTinggi);

            cat.setStyle(Paint.Style.FILL);
            cat.setColor(ROW);
            canvas.drawRoundRect(track, dp(ctx, 3f), dp(ctx, 3f), cat);

            cat.setStyle(Paint.Style.STROKE);
            cat.setStrokeWidth(dp(ctx, 1f));
            cat.setColor(TRACK);
            canvas.drawRoundRect(track, dp(ctx, 3f), dp(ctx, 3f), cat);

            float thumb = dp(ctx, 14f);
            float jarak = dp(ctx, 2f);
            float kiri = isChecked() ? lebar - thumb - jarak : jarak;
            float thumbAtas = (tinggi - thumb) / 2f;
            RectF tombol = new RectF(kiri, thumbAtas, kiri + thumb, thumbAtas + thumb);

            cat.setStyle(Paint.Style.FILL);
            cat.setColor(isChecked() ? ICON_BOX : TRACK);
            canvas.drawRoundRect(tombol, dp(ctx, 2f), dp(ctx, 2f), cat);

            cat.setStyle(Paint.Style.STROKE);
            cat.setStrokeWidth(dp(ctx, 1f));
            cat.setColor(TITLE_TEXT);
            canvas.drawRoundRect(tombol, dp(ctx, 2f), dp(ctx, 2f), cat);
        }
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

    /** Tanaman rambat hijau tipis di tepi kiri dan kanan body panel. */
    public static class VineDrawable extends Drawable {
        private final Paint cat = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Paint latar = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final Context ctx;

        public VineDrawable(Context context) {
            ctx = context;
            latar.setColor(BODY);
            latar.setStyle(Paint.Style.FILL);
            cat.setColor(VINE);
            cat.setStyle(Paint.Style.STROKE);
            cat.setStrokeWidth(dp(ctx, 2f));
        }

        @Override
        public void draw(Canvas canvas) {
            Rect bounds = getBounds();
            canvas.drawRect(bounds, latar);

            float lebar = bounds.width();
            float tinggi = bounds.height();
            for (int sisi = 0; sisi < 2; sisi++) {
                float tepi = sisi == 0 ? bounds.left + dp(ctx, 4f)
                        : bounds.right - dp(ctx, 4f);
                float arah = sisi == 0 ? 1f : -1f;

                Path batang = new Path();
                batang.moveTo(tepi, bounds.top);
                float y = bounds.top;
                int i = 0;
                while (y < bounds.bottom) {
                    y += dp(ctx, 26f);
                    batang.lineTo(tepi + arah * (i % 2 == 0 ? dp(ctx, 3f) : -dp(ctx, 2f)),
                            Math.min(y, bounds.bottom));
                    i++;
                }
                canvas.drawPath(batang, cat);

                cat.setStyle(Paint.Style.FILL);
                y = bounds.top + dp(ctx, 14f);
                i = 0;
                while (y < bounds.bottom) {
                    float daun = dp(ctx, 4f);
                    float x = tepi + arah * (i % 2 == 0 ? dp(ctx, 4f) : -dp(ctx, 1f));
                    Path bentuk = new Path();
                    bentuk.moveTo(x, y - daun);
                    bentuk.lineTo(x + arah * daun, y);
                    bentuk.lineTo(x, y + daun);
                    bentuk.lineTo(x - arah * daun * 0.4f, y);
                    bentuk.close();
                    canvas.drawPath(bentuk, cat);
                    y += dp(ctx, 26f);
                    i++;
                }
                cat.setStyle(Paint.Style.STROKE);
            }
        }

        @Override
        public void setAlpha(int alpha) {
            cat.setAlpha(alpha);
            latar.setAlpha(alpha);
            invalidateSelf();
        }

        @Override
        public void setColorFilter(android.graphics.ColorFilter colorFilter) {
            cat.setColorFilter(colorFilter);
            invalidateSelf();
        }

        @Override
        public int getOpacity() {
            return PixelFormat.TRANSLUCENT;
        }
    }

    /**
     * Ikon kecil bergaya pixel. Kotak cokelat tua digambar oleh view ini, jadi
     * baris tidak perlu menyusun dua lapis background.
     */
    public static class Ikon extends View {
        public static final int TOGGLE = 0;
        public static final int SLIDER = 1;
        public static final int BUTTON = 2;
        public static final int TEXT = 3;
        public static final int PLAYER = 4;
        public static final int SPINNER = 5;
        public static final int CHECK = 6;
        public static final int RADIO = 7;
        public static final int PANAH_KANAN = 8;
        public static final int PANAH_BAWAH = 9;
        public static final int BOOKMARK = 10;
        public static final int CLOSE = 11;

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
            float radius = dp(ctx, 3f);

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
            float kuas = dp(ctx, 1.5f);

            switch (jenis) {
                case TOGGLE: {
                    float trackL = s * 0.56f;
                    float trackT = s * 0.26f;
                    RectF track = new RectF(cx - trackL / 2f, cy - trackT / 2f,
                            cx + trackL / 2f, cy + trackT / 2f);
                    cat.setStyle(Paint.Style.STROKE);
                    cat.setStrokeWidth(dp(ctx, 1f));
                    canvas.drawRoundRect(track, dp(ctx, 2f), dp(ctx, 2f), cat);

                    float knob = trackT - dp(ctx, 2f);
                    float kiri = cx + trackL / 2f - dp(ctx, 1f) - knob;
                    cat.setStyle(Paint.Style.FILL);
                    canvas.drawRoundRect(kiri, cy - knob / 2f, kiri + knob, cy + knob / 2f,
                            dp(ctx, 1f), dp(ctx, 1f), cat);
                    break;
                }
                case SLIDER: {
                    float panjang = s * 0.52f;
                    cat.setStyle(Paint.Style.FILL);
                    for (int i = -1; i <= 1; i++) {
                        float y = cy + i * s * 0.17f;
                        canvas.drawRect(cx - panjang / 2f, y, cx + panjang / 2f,
                                y + dp(ctx, 1.5f), cat);
                    }
                    float knob = dp(ctx, 4f);
                    canvas.drawRect(cx - panjang / 4f - knob / 2f, cy - s * 0.17f - knob / 2f,
                            cx - panjang / 4f + knob / 2f, cy - s * 0.17f + knob / 2f, cat);
                    canvas.drawRect(cx + panjang / 4f - knob / 2f, cy + s * 0.17f - knob / 2f,
                            cx + panjang / 4f + knob / 2f, cy + s * 0.17f + knob / 2f, cat);
                    break;
                }
                case BUTTON:
                case PANAH_KANAN:
                case PANAH_BAWAH: {
                    Path path = new Path();
                    if (jenis == PANAH_BAWAH) {
                        path.moveTo(cx - s * 0.22f, cy - s * 0.12f);
                        path.lineTo(cx + s * 0.22f, cy - s * 0.12f);
                        path.lineTo(cx, cy + s * 0.2f);
                    } else {
                        path.moveTo(cx - s * 0.16f, cy - s * 0.22f);
                        path.lineTo(cx + s * 0.22f, cy);
                        path.lineTo(cx - s * 0.16f, cy + s * 0.22f);
                    }
                    path.close();
                    cat.setStyle(Paint.Style.FILL);
                    canvas.drawPath(path, cat);
                    break;
                }
                case TEXT: {
                    cat.setStyle(Paint.Style.FILL);
                    float lebar1 = s * 0.52f;
                    for (int i = -1; i <= 1; i++) {
                        float y = cy + i * s * 0.17f;
                        float panjang = i == 1 ? lebar1 * 0.65f : lebar1;
                        canvas.drawRect(cx - lebar1 / 2f, y,
                                cx - lebar1 / 2f + panjang, y + dp(ctx, 1.5f), cat);
                    }
                    break;
                }
                case PLAYER: {
                    cat.setStyle(Paint.Style.FILL);
                    canvas.drawCircle(cx, cy - s * 0.13f, s * 0.12f, cat);
                    RectF badan = new RectF(cx - s * 0.22f, cy + s * 0.02f,
                            cx + s * 0.22f, cy + s * 0.26f);
                    canvas.drawRoundRect(badan, dp(ctx, 4f), dp(ctx, 4f), cat);
                    break;
                }
                case SPINNER: {
                    cat.setStyle(Paint.Style.STROKE);
                    cat.setStrokeWidth(kuas);
                    Path path = new Path();
                    path.moveTo(cx - s * 0.15f, cy - s * 0.06f);
                    path.lineTo(cx, cy + s * 0.1f);
                    path.lineTo(cx + s * 0.15f, cy - s * 0.06f);
                    canvas.drawPath(path, cat);
                    break;
                }
                case CHECK: {
                    cat.setStyle(Paint.Style.STROKE);
                    cat.setStrokeWidth(kuas);
                    Path path = new Path();
                    path.moveTo(cx - s * 0.18f, cy);
                    path.lineTo(cx - s * 0.02f, cy + s * 0.16f);
                    path.lineTo(cx + s * 0.2f, cy - s * 0.16f);
                    canvas.drawPath(path, cat);
                    break;
                }
                case RADIO: {
                    cat.setStyle(Paint.Style.STROKE);
                    cat.setStrokeWidth(kuas);
                    canvas.drawCircle(cx, cy, s * 0.18f, cat);
                    cat.setStyle(Paint.Style.FILL);
                    canvas.drawCircle(cx, cy, s * 0.07f, cat);
                    break;
                }
                case BOOKMARK: {
                    float kiri = cx - s * 0.14f;
                    float kanan = cx + s * 0.14f;
                    Path path = new Path();
                    path.moveTo(kiri, cy - s * 0.2f);
                    path.lineTo(kanan, cy - s * 0.2f);
                    path.lineTo(kanan, cy + s * 0.2f);
                    path.lineTo(cx, cy + s * 0.08f);
                    path.lineTo(kiri, cy + s * 0.2f);
                    path.close();
                    cat.setStyle(Paint.Style.FILL);
                    canvas.drawPath(path, cat);
                    break;
                }
                case CLOSE: {
                    cat.setStyle(Paint.Style.STROKE);
                    cat.setStrokeWidth(kuas);
                    float ujung = s * 0.15f;
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
