package com.android.support;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.os.SystemClock;
import android.view.animation.DecelerateInterpolator;
import android.view.animation.OvershootInterpolator;
import android.widget.CompoundButton;

/**
 * Sakelar pil gaya game: pil 52x26dp, garis 3dp #B28950, knob bulat 20dp yang
 * mengisi penuh bagian dalam pil.
 *
 * MATI : track #B28950, knob #CEA972 di kiri.
 * HIDUP: track #F8B551, knob #FFF799 di kanan.
 *
 * Tetap turunan CompoundButton supaya aksesibilitas dan penanganan klik sama
 * seperti Switch bawaan. View-nya setinggi 48dp agar area sentuh memenuhi
 * ukuran minimum, pilnya digambar di tengah.
 *
 * Animasi digerakkan dari onDraw, bukan ValueAnimator, karena tiga hal yang
 * membuat animasi sebelumnya kadang tidak terlihat:
 *
 * 1. ValueAnimator mengikuti "animator duration scale" sistem. Dengan penghemat
 *    baterai atau opsi pengembang bernilai 0, animasi langsung selesai.
 * 2. Listener perubahan dipanggil sebelum animasi mulai. Bila pekerjaan di
 *    listener (JNI ke native) menahan thread utama, frame pertama datang
 *    terlambat dan ValueAnimator melompat ke akhir. Sekarang animasi mulai
 *    lebih dulu dan listener dipanggil sesudah beberapa frame.
 * 3. Terlepas dari window di tengah animasi meninggalkan knob setengah jalan.
 *    Sekarang posisi selalu dikembalikan ke status sebenarnya.
 *
 * Kemajuan per frame dibatasi {@link #BATAS_FRAME_MS}, jadi sekalipun satu
 * frame tertahan, knob tetap bergerak bertahap dan tidak lompat.
 */
public class AnimatedSwitch extends CompoundButton {
    private static final float DURASI = 190f;
    private static final float DURASI_TEKAN = 90f;
    private static final float BATAS_FRAME_MS = 20f;
    private static final long JEDA_KABAR_MS = 50L;

    private static final float PIL_LEBAR_DP = 52f;
    private static final float PIL_TINGGI_DP = 26f;
    private static final float GARIS_DP = 3f;
    private static final float TEKAN_MELAR_DP = 4f;
    private static final float TARGET_TINGGI_DP = Theme.TARGET_DP;

    private final Paint cat = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF bidang = new RectF();
    private final OvershootInterpolator memantul = new OvershootInterpolator(0.9f);
    private final DecelerateInterpolator mengerem = new DecelerateInterpolator();

    // posisi 0 = mati, 1 = hidup (boleh sedikit melewati batas saat overshoot).
    private float posisi;
    // tekan 0..1 = knob melar saat jari menyentuh.
    private float tekan;
    private float tekanTujuan;

    private boolean geser;
    private float geserAwal;
    private float geserTujuan;
    private float geserProgres;
    private float geserDurasi;
    private boolean geserMemantul;

    private boolean berjalan;
    private long terakhirMs;

    // Ukuran dalam piksel dihitung sekali di konstruktor, bukan tiap frame.
    private final float pilLebarPx;
    private final float pilTinggiPx;
    private final float garisPx;
    private final float melarPx;
    private final int targetTinggiPx;

    private OnCheckedChangeListener pendengar;
    private boolean terakhirDikabarkan;

    private final Runnable kabarkan = new Runnable() {
        @Override
        public void run() {
            boolean sekarang = isChecked();
            if (sekarang == terakhirDikabarkan)
                return;
            terakhirDikabarkan = sekarang;
            if (pendengar != null)
                pendengar.onCheckedChanged(AnimatedSwitch.this, sekarang);
        }
    };

    public AnimatedSwitch(Context context) {
        super(context);
        pilLebarPx = Theme.dp(context, PIL_LEBAR_DP);
        pilTinggiPx = Theme.dp(context, PIL_TINGGI_DP);
        garisPx = Theme.dp(context, GARIS_DP);
        melarPx = Theme.dp(context, TEKAN_MELAR_DP);
        targetTinggiPx = (int) Theme.dp(context, TARGET_TINGGI_DP);
        setBackground(null);
        setButtonDrawable(null);
        setPadding(0, 0, 0, 0);
        setClickable(true);
        setFocusable(true);
        posisi = isChecked() ? 1f : 0f;
    }

    /**
     * Listener tidak diteruskan ke CompoundButton supaya tidak dipanggil di
     * tengah setChecked, sebelum animasi mulai.
     */
    @Override
    public void setOnCheckedChangeListener(OnCheckedChangeListener listener) {
        pendengar = listener;
        terakhirDikabarkan = isChecked();
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        setMeasuredDimension(resolveSize((int) pilLebarPx, widthMeasureSpec),
                resolveSize(targetTinggiPx, heightMeasureSpec));
    }

    /**
     * Nilai dari restore dipasang tanpa animasi dan tanpa kabar, karena view
     * belum punya ukuran saat Preferences.loadPrefBool dipanggil. Ketukan
     * sesudah layout selalu dianimasikan dari posisi sekarang, jadi ketukan
     * cepat bisa dibalik tanpa lompat.
     */
    @Override
    public void setChecked(boolean checked) {
        super.setChecked(checked);

        float tujuan = checked ? 1f : 0f;
        boolean terlihat = isLaidOut() && isShown();
        if (!terlihat) {
            hentikanAnimasi();
            posisi = tujuan;
            invalidate();
            return;
        }

        if (geser ? geserTujuan != tujuan : posisi != tujuan)
            mulaiGeser(tujuan);

        // Kabar ditunda beberapa frame agar knob sudah mulai bergerak sebelum
        // pekerjaan berat di listener. Ketukan beruntun digabung: yang dikabarkan
        // hanya keadaan akhir, dan hanya bila berbeda dari kabar terakhir.
        removeCallbacks(kabarkan);
        postDelayed(kabarkan, JEDA_KABAR_MS);
    }

    @Override
    protected void drawableStateChanged() {
        super.drawableStateChanged();
        tekanTujuan = isPressed() ? 1f : 0f;
        if (isLaidOut() && tekan != tekanTujuan)
            mulaiFrame();
        invalidate();
    }

    private void mulaiGeser(float tujuan) {
        float jarak = Math.abs(tujuan - posisi);
        geserAwal = posisi;
        geserTujuan = tujuan;
        geserProgres = 0f;
        geserDurasi = DURASI * Math.max(0.45f, jarak);
        // Geser jauh sedikit memantul di ujung seperti saklar fisik; geser
        // pendek (membalik di tengah jalan) hanya mengerem.
        geserMemantul = jarak > 0.6f;
        geser = true;
        mulaiFrame();
    }

    private void mulaiFrame() {
        if (!berjalan) {
            berjalan = true;
            terakhirMs = SystemClock.uptimeMillis();
        }
        postInvalidateOnAnimation();
    }

    /** Memajukan animasi satu frame. True bila masih perlu frame berikutnya. */
    private boolean langkah() {
        long sekarang = SystemClock.uptimeMillis();
        float dt = Math.min((float) (sekarang - terakhirMs), BATAS_FRAME_MS);
        terakhirMs = sekarang;
        boolean lanjut = false;

        if (geser) {
            geserProgres += dt / geserDurasi;
            if (geserProgres >= 1f) {
                posisi = geserTujuan;
                geser = false;
            } else {
                float t = geserMemantul
                        ? memantul.getInterpolation(geserProgres)
                        : mengerem.getInterpolation(geserProgres);
                posisi = geserAwal + (geserTujuan - geserAwal) * t;
                lanjut = true;
            }
        }

        if (tekan != tekanTujuan) {
            float maju = dt / DURASI_TEKAN;
            tekan = tekan < tekanTujuan
                    ? Math.min(tekanTujuan, tekan + maju)
                    : Math.max(tekanTujuan, tekan - maju);
            if (tekan != tekanTujuan)
                lanjut = true;
        }
        return lanjut;
    }

    /** Menghentikan animasi dan mengembalikan tampilan ke status yang sebenarnya. */
    private void hentikanAnimasi() {
        geser = false;
        berjalan = false;
        tekan = 0f;
        tekanTujuan = 0f;
        posisi = isChecked() ? 1f : 0f;
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        hentikanAnimasi();
    }

    @Override
    protected void onDetachedFromWindow() {
        // Kabar yang tertunda dikirim sekarang supaya tidak hilang, dan knob
        // tidak tertinggal setengah jalan.
        removeCallbacks(kabarkan);
        kabarkan.run();
        hentikanAnimasi();
        super.onDetachedFromWindow();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        boolean lanjut = berjalan && langkah();
        if (lanjut)
            postInvalidateOnAnimation();
        else
            berjalan = false;

        float lebar = getWidth();
        float tinggi = getHeight();
        float pilLebar = Math.min(lebar, pilLebarPx);
        float pilTinggi = pilTinggiPx;
        float garis = garisPx;

        float kiri = (lebar - pilLebar) / 2f;
        float atas = (tinggi - pilTinggi) / 2f;
        float radius = pilTinggi / 2f;

        // Warna mengikuti posisi tetapi dijepit supaya overshoot tidak membuat
        // warna keluar dari rentang. Theme.campur tidak membuat objek baru.
        float f = Math.max(0f, Math.min(1f, posisi));
        int warnaTrack = Theme.campur(Theme.SWITCH_OFF_TRACK, Theme.SWITCH_ON_TRACK, f);
        int warnaKnob = Theme.campur(Theme.SWITCH_OFF_KNOB, Theme.SWITCH_ON_KNOB, f);

        // Garis luar #B28950 sebagai pil penuh, lalu track di dalamnya. Saat mati
        // track memang berwarna sama dengan garis, persis seperti di game.
        cat.setStyle(Paint.Style.FILL);
        cat.setColor(Theme.STROKE_KONTROL);
        bidang.set(kiri, atas, kiri + pilLebar, atas + pilTinggi);
        canvas.drawRoundRect(bidang, radius, radius, cat);

        cat.setColor(warnaTrack);
        bidang.set(kiri + garis, atas + garis, kiri + pilLebar - garis, atas + pilTinggi - garis);
        float radiusDalam = radius - garis;
        canvas.drawRoundRect(bidang, radiusDalam, radiusDalam, cat);

        // Knob bulat mengisi penuh tinggi bagian dalam. Saat ditekan knob melar
        // ke arah tengah supaya tepi luarnya tidak lompat.
        float diameter = pilTinggi - 2f * garis;
        float melar = melarPx * tekan;
        float jalur = pilLebar - 2f * garis - diameter;
        float knobKiri = kiri + garis + jalur * posisi - melar * posisi;
        bidang.set(knobKiri, atas + garis, knobKiri + diameter + melar, atas + garis + diameter);

        cat.setColor(warnaKnob);
        canvas.drawRoundRect(bidang, diameter / 2f, diameter / 2f, cat);
    }
}
