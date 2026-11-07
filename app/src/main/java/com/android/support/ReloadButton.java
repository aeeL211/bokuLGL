package com.android.support;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.util.TypedValue;
import android.view.View;

/**
 * Tombol "Muat ulang" berlabel untuk daftar pemain, dengan tiga keadaan yang
 * terlihat jelas:
 * <ul>
 * <li>diam: glyph dan teks "Muat ulang" di atas kotak kayu gelap;</li>
 * <li>memuat: glyph berputar per 90 derajat dan teks "Memuat…". Putaran 90 derajat
 * kelipatan bulat menjaga piksel tetap tajam, dan glyph simetris terhadap
 * putaran 180 derajat sehingga tidak ada lompatan;</li>
 * <li>selesai: kotak berubah terang seperti switch aktif dan teks "Diperbarui",
 * lalu kembali ke keadaan diam.</li>
 * </ul>
 * Kotaknya setinggi {@link #TINGGI_DP}, sedangkan view-nya {@link Theme#TARGET_DP}
 * (48dp) supaya area sentuhnya cukup. Lebarnya tetap untuk ketiga label sehingga
 * header tidak bergeser saat label berganti.
 */
final class ReloadButton extends View {
    private static final int DIAM = 0;
    private static final int MEMUAT = 1;
    private static final int SELESAI = 2;

    private static final String LABEL_IDLE = "Muat ulang";
    private static final String LABEL_LOADING = "Memuat…";
    private static final String LABEL_DONE = "Diperbarui";

    private static final float HEIGHT_DP = 34f;
    private static final float LINE_DP = 2f;
    private static final float PAD_DP = 10f;
    private static final float GLYPH_DP = 20f;
    private static final float JARAK_DP = 6f;

    /** Satu langkah putaran (90 derajat) tiap LANGKAH_MS; 6 langkah = satu setengah putaran. */
    private static final long STEP_MS = 130L;
    private static final int COUNT_STEP = 6;
    private static final long SHOW_DONE_MS = 1000L;

    private final Runnable action;
    private final Paint cat = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint catText = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint catGlyph = PixelArt.catTajam();
    private final RectF box = new RectF();
    private final RectF line = new RectF();
    private final Rect target = new Rect();
    private final Bitmap glyphBright;
    private final Bitmap glyphDark;
    private final float heightPx;
    private final float linePx;
    private final float radiusPx;
    private final float padPx;
    private final float jarakPx;
    private final int sel;
    private final int glyphPx;

    private int keadaan = DIAM;
    private int rotate;

    private final Runnable step = new Runnable() {
        @Override
        public void run() {
            rotate++;
            if (rotate >= COUNT_STEP) {
                change(SELESAI);
                postDelayed(back, SHOW_DONE_MS);
            } else {
                invalidate();
                postDelayed(this, STEP_MS);
            }
        }
    };

    private final Runnable back = new Runnable() {
        @Override
        public void run() {
            change(DIAM);
        }
    };

    /**
     * @param pola glyph 16x16 yang simetris terhadap putaran 180 derajat
     * @param aksi dijalankan di thread utama setiap kali tombol diketuk
     */
    ReloadButton(Context context, String[] pattern, Runnable action) {
        super(context);
        this.action = action;
        glyphBright = PixelArt.create(pattern, Theme.TITLE_TEXT, Theme.TITLE_TEXT, Theme.TITLE_TEXT);
        glyphDark = PixelArt.create(pattern, Theme.TEXT, Theme.TEXT, Theme.TEXT);
        heightPx = Theme.dp(context, HEIGHT_DP);
        linePx = Theme.dp(context, LINE_DP);
        radiusPx = Theme.dp(context, Theme.RADIUS_PANEL);
        padPx = Theme.dp(context, PAD_DP);
        jarakPx = Theme.dp(context, JARAK_DP);
        sel = Math.max(1, (int) Math.floor(Theme.dp(context, GLYPH_DP) / PixelArt.COLUMNS));
        glyphPx = sel * PixelArt.COLUMNS;

        catText.setTypeface(Theme.font(context));
        catText.setTextSize(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_SP, Theme.DESC_SP,
                context.getResources().getDisplayMetrics()));
        catText.setTextAlign(Paint.Align.CENTER);

        setClickable(true);
        setFocusable(true);
        setContentDescription("Muat ulang daftar pemain");
        setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                start();
            }
        });
    }

    private void start() {
        removeCallbacks(step);
        removeCallbacks(back);
        if (action != null)
            action.run();
        rotate = 0;
        if (!Transition.active(getContext())) {
            // Animasi sistem mati: langsung tampilkan hasilnya tanpa putaran.
            change(SELESAI);
            postDelayed(back, SHOW_DONE_MS);
            return;
        }
        change(MEMUAT);
        postDelayed(step, STEP_MS);
    }

    private void change(int fresh) {
        keadaan = fresh;
        if (fresh == DIAM)
            rotate = 0;
        setContentDescription(fresh == MEMUAT ? "Memuat daftar pemain"
                : fresh == SELESAI ? "Daftar pemain diperbarui" : "Muat ulang daftar pemain");
        invalidate();
    }

    private String label() {
        return keadaan == MEMUAT ? LABEL_LOADING : keadaan == SELESAI ? LABEL_DONE : LABEL_IDLE;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        // Lebar mengikuti label terpanjang supaya ukuran tombol tidak berubah saat label berganti.
        float text = Math.max(catText.measureText(LABEL_IDLE),
                Math.max(catText.measureText(LABEL_LOADING), catText.measureText(LABEL_DONE)));
        int width = (int) Math.ceil(padPx * 2 + glyphPx + jarakPx + text);
        int height = (int) Theme.dp(getContext(), Theme.TARGET_DP);
        setMeasuredDimension(resolveSize(width, widthMeasureSpec),
                resolveSize(height, heightMeasureSpec));
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        float top = (h - heightPx) / 2f;
        box.set(0f, top, w, top + heightPx);
        float enter = linePx / 2f;
        line.set(box.left + enter, box.top + enter, box.right - enter, box.bottom - enter);
    }

    @Override
    protected void drawableStateChanged() {
        super.drawableStateChanged();
        invalidate();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        boolean done = keadaan == SELESAI;
        cat.setStyle(Paint.Style.FILL);
        cat.setColor(done ? Theme.SWITCH_ON_TRACK : isPressed() ? Theme.TRACK : Theme.ICON_BOX);
        canvas.drawRoundRect(box, radiusPx, radiusPx, cat);

        cat.setStyle(Paint.Style.STROKE);
        cat.setStrokeWidth(linePx);
        cat.setColor(Theme.BUTTON_BORDER);
        canvas.drawRoundRect(line, radiusPx, radiusPx, cat);

        int height = getHeight();
        int labelColor = done ? Theme.TEXT : Theme.TITLE_TEXT;

        // Glyph: kotak sisi glyphPx di sisi kiri, diputar di titik tengahnya sendiri.
        canvas.save();
        canvas.translate(Math.round(padPx), Math.round((height - glyphPx) / 2f));
        if (keadaan == MEMUAT)
            canvas.rotate(90f * (rotate % 4), glyphPx / 2f, glyphPx / 2f);
        PixelArt.image(canvas, done ? glyphDark : glyphBright, catGlyph, target,
                glyphPx, glyphPx, sel);
        canvas.restore();

        float leftText = padPx + glyphPx + jarakPx;
        float centerText = leftText + (getWidth() - padPx - leftText) / 2f;
        Paint.FontMetrics fm = catText.getFontMetrics();
        float lineBase = height / 2f - (fm.ascent + fm.descent) / 2f;
        catText.setColor(labelColor);
        canvas.drawText(label(), centerText, lineBase, catText);
    }

    @Override
    protected void onDetachedFromWindow() {
        removeCallbacks(step);
        removeCallbacks(back);
        // Bila dipasang kembali, jangan tersangkut di tengah animasi.
        keadaan = DIAM;
        rotate = 0;
        super.onDetachedFromWindow();
    }
}
