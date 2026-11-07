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
final class TombolMuatUlang extends View {
    private static final int DIAM = 0;
    private static final int MEMUAT = 1;
    private static final int SELESAI = 2;

    private static final String LABEL_DIAM = "Muat ulang";
    private static final String LABEL_MEMUAT = "Memuat…";
    private static final String LABEL_SELESAI = "Diperbarui";

    private static final float TINGGI_DP = 34f;
    private static final float GARIS_DP = 2f;
    private static final float PAD_DP = 10f;
    private static final float GLYPH_DP = 20f;
    private static final float JARAK_DP = 6f;

    /** Satu langkah putaran (90 derajat) tiap LANGKAH_MS; 6 langkah = satu setengah putaran. */
    private static final long LANGKAH_MS = 130L;
    private static final int JUMLAH_LANGKAH = 6;
    private static final long TAMPIL_SELESAI_MS = 1000L;

    private final Runnable aksi;
    private final Paint cat = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint catTeks = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint catGlyph = Piksel.catTajam();
    private final RectF kotak = new RectF();
    private final RectF garis = new RectF();
    private final Rect tujuan = new Rect();
    private final Bitmap glyphTerang;
    private final Bitmap glyphGelap;
    private final float tinggiPx;
    private final float garisPx;
    private final float radiusPx;
    private final float padPx;
    private final float jarakPx;
    private final int sel;
    private final int glyphPx;

    private int keadaan = DIAM;
    private int putar;

    private final Runnable langkah = new Runnable() {
        @Override
        public void run() {
            putar++;
            if (putar >= JUMLAH_LANGKAH) {
                ubah(SELESAI);
                postDelayed(kembali, TAMPIL_SELESAI_MS);
            } else {
                invalidate();
                postDelayed(this, LANGKAH_MS);
            }
        }
    };

    private final Runnable kembali = new Runnable() {
        @Override
        public void run() {
            ubah(DIAM);
        }
    };

    /**
     * @param pola glyph 16x16 yang simetris terhadap putaran 180 derajat
     * @param aksi dijalankan di thread utama setiap kali tombol diketuk
     */
    TombolMuatUlang(Context context, String[] pola, Runnable aksi) {
        super(context);
        this.aksi = aksi;
        glyphTerang = Piksel.buat(pola, Theme.TITLE_TEXT, Theme.TITLE_TEXT, Theme.TITLE_TEXT);
        glyphGelap = Piksel.buat(pola, Theme.TEXT, Theme.TEXT, Theme.TEXT);
        tinggiPx = Theme.dp(context, TINGGI_DP);
        garisPx = Theme.dp(context, GARIS_DP);
        radiusPx = Theme.dp(context, Theme.RADIUS_PANEL);
        padPx = Theme.dp(context, PAD_DP);
        jarakPx = Theme.dp(context, JARAK_DP);
        sel = Math.max(1, (int) Math.floor(Theme.dp(context, GLYPH_DP) / Piksel.KOLOM));
        glyphPx = sel * Piksel.KOLOM;

        catTeks.setTypeface(Theme.font(context));
        catTeks.setTextSize(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_SP, Theme.DESC_SP,
                context.getResources().getDisplayMetrics()));
        catTeks.setTextAlign(Paint.Align.CENTER);

        setClickable(true);
        setFocusable(true);
        setContentDescription("Muat ulang daftar pemain");
        setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mulai();
            }
        });
    }

    private void mulai() {
        removeCallbacks(langkah);
        removeCallbacks(kembali);
        if (aksi != null)
            aksi.run();
        putar = 0;
        if (!Transisi.aktif(getContext())) {
            // Animasi sistem mati: langsung tampilkan hasilnya tanpa putaran.
            ubah(SELESAI);
            postDelayed(kembali, TAMPIL_SELESAI_MS);
            return;
        }
        ubah(MEMUAT);
        postDelayed(langkah, LANGKAH_MS);
    }

    private void ubah(int baru) {
        keadaan = baru;
        if (baru == DIAM)
            putar = 0;
        setContentDescription(baru == MEMUAT ? "Memuat daftar pemain"
                : baru == SELESAI ? "Daftar pemain diperbarui" : "Muat ulang daftar pemain");
        invalidate();
    }

    private String label() {
        return keadaan == MEMUAT ? LABEL_MEMUAT : keadaan == SELESAI ? LABEL_SELESAI : LABEL_DIAM;
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        // Lebar mengikuti label terpanjang supaya ukuran tombol tidak berubah saat label berganti.
        float teks = Math.max(catTeks.measureText(LABEL_DIAM),
                Math.max(catTeks.measureText(LABEL_MEMUAT), catTeks.measureText(LABEL_SELESAI)));
        int lebar = (int) Math.ceil(padPx * 2 + glyphPx + jarakPx + teks);
        int tinggi = (int) Theme.dp(getContext(), Theme.TARGET_DP);
        setMeasuredDimension(resolveSize(lebar, widthMeasureSpec),
                resolveSize(tinggi, heightMeasureSpec));
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        float atas = (h - tinggiPx) / 2f;
        kotak.set(0f, atas, w, atas + tinggiPx);
        float masuk = garisPx / 2f;
        garis.set(kotak.left + masuk, kotak.top + masuk, kotak.right - masuk, kotak.bottom - masuk);
    }

    @Override
    protected void drawableStateChanged() {
        super.drawableStateChanged();
        invalidate();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        boolean selesai = keadaan == SELESAI;
        cat.setStyle(Paint.Style.FILL);
        cat.setColor(selesai ? Theme.SWITCH_ON_TRACK : isPressed() ? Theme.TRACK : Theme.ICON_BOX);
        canvas.drawRoundRect(kotak, radiusPx, radiusPx, cat);

        cat.setStyle(Paint.Style.STROKE);
        cat.setStrokeWidth(garisPx);
        cat.setColor(Theme.BUTTON_BORDER);
        canvas.drawRoundRect(garis, radiusPx, radiusPx, cat);

        int tinggi = getHeight();
        int teksWarna = selesai ? Theme.TEXT : Theme.TITLE_TEXT;

        // Glyph: kotak sisi glyphPx di sisi kiri, diputar di titik tengahnya sendiri.
        canvas.save();
        canvas.translate(Math.round(padPx), Math.round((tinggi - glyphPx) / 2f));
        if (keadaan == MEMUAT)
            canvas.rotate(90f * (putar % 4), glyphPx / 2f, glyphPx / 2f);
        Piksel.gambar(canvas, selesai ? glyphGelap : glyphTerang, catGlyph, tujuan,
                glyphPx, glyphPx, sel);
        canvas.restore();

        float kiriTeks = padPx + glyphPx + jarakPx;
        float tengahTeks = kiriTeks + (getWidth() - padPx - kiriTeks) / 2f;
        Paint.FontMetrics fm = catTeks.getFontMetrics();
        float garisDasar = tinggi / 2f - (fm.ascent + fm.descent) / 2f;
        catTeks.setColor(teksWarna);
        canvas.drawText(label(), tengahTeks, garisDasar, catTeks);
    }

    @Override
    protected void onDetachedFromWindow() {
        removeCallbacks(langkah);
        removeCallbacks(kembali);
        // Bila dipasang kembali, jangan tersangkut di tengah animasi.
        keadaan = DIAM;
        putar = 0;
        super.onDetachedFromWindow();
    }
}
