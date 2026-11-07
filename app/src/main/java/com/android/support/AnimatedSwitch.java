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
    private static final float DURATION_PRESS = 90f;
    private static final float LIMIT_FRAME_MS = 20f;
    private static final long DELAY_NOTIFY_MS = 50L;

    private static final float PILL_WIDTH_DP = 52f;
    private static final float PILL_HEIGHT_DP = 26f;
    private static final float LINE_DP = 3f;
    private static final float PRESS_STRETCH_DP = 4f;
    private static final float TARGET_HEIGHT_DP = Theme.TARGET_DP;

    private final Paint cat = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF field = new RectF();
    private final OvershootInterpolator bounce = new OvershootInterpolator(0.9f);
    private final DecelerateInterpolator mengerem = new DecelerateInterpolator();

    // posisi 0 = mati, 1 = hidup (boleh sedikit melewati batas saat overshoot).
    private float position;
    // tekan 0..1 = knob melar saat jari menyentuh.
    private float press;
    private float pressTarget;

    private boolean drag;
    private float dragInitial;
    private float dragTarget;
    private float dragProgress;
    private float dragDuration;
    private boolean dragBounce;

    private boolean running;
    private long lastMs;

    // Ukuran dalam piksel dihitung sekali di konstruktor, bukan tiap frame.
    private final float pillWidthPx;
    private final float pillHeightPx;
    private final float linePx;
    private final float stretchPx;
    private final int targetHeightPx;

    private OnCheckedChangeListener pendengar;
    private boolean lastDikabarkan;

    private final Runnable kabarkan = new Runnable() {
        @Override
        public void run() {
            boolean now = isChecked();
            if (now == lastDikabarkan)
                return;
            lastDikabarkan = now;
            if (pendengar != null)
                pendengar.onCheckedChanged(AnimatedSwitch.this, now);
        }
    };

    public AnimatedSwitch(Context context) {
        super(context);
        pillWidthPx = Theme.dp(context, PILL_WIDTH_DP);
        pillHeightPx = Theme.dp(context, PILL_HEIGHT_DP);
        linePx = Theme.dp(context, LINE_DP);
        stretchPx = Theme.dp(context, PRESS_STRETCH_DP);
        targetHeightPx = (int) Theme.dp(context, TARGET_HEIGHT_DP);
        setBackground(null);
        setButtonDrawable(null);
        setPadding(0, 0, 0, 0);
        setClickable(true);
        setFocusable(true);
        position = isChecked() ? 1f : 0f;
    }

    /**
     * Listener tidak diteruskan ke CompoundButton supaya tidak dipanggil di
     * tengah setChecked, sebelum animasi mulai.
     */
    @Override
    public void setOnCheckedChangeListener(OnCheckedChangeListener listener) {
        pendengar = listener;
        lastDikabarkan = isChecked();
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        setMeasuredDimension(resolveSize((int) pillWidthPx, widthMeasureSpec),
                resolveSize(targetHeightPx, heightMeasureSpec));
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

        float target = checked ? 1f : 0f;
        boolean terlihat = isLaidOut() && isShown();
        if (!terlihat) {
            stopAnimation();
            position = target;
            invalidate();
            return;
        }

        if (drag ? dragTarget != target : position != target)
            startDrag(target);

        // Kabar ditunda beberapa frame agar knob sudah mulai bergerak sebelum
        // pekerjaan berat di listener. Ketukan beruntun digabung: yang dikabarkan
        // hanya keadaan akhir, dan hanya bila berbeda dari kabar terakhir.
        removeCallbacks(kabarkan);
        postDelayed(kabarkan, DELAY_NOTIFY_MS);
    }

    @Override
    protected void drawableStateChanged() {
        super.drawableStateChanged();
        pressTarget = isPressed() ? 1f : 0f;
        if (isLaidOut() && press != pressTarget)
            startFrame();
        invalidate();
    }

    private void startDrag(float target) {
        float jarak = Math.abs(target - position);
        dragInitial = position;
        dragTarget = target;
        dragProgress = 0f;
        dragDuration = DURASI * Math.max(0.45f, jarak);
        // Geser jauh sedikit memantul di ujung seperti saklar fisik; geser
        // pendek (membalik di tengah jalan) hanya mengerem.
        dragBounce = jarak > 0.6f;
        drag = true;
        startFrame();
    }

    private void startFrame() {
        if (!running) {
            running = true;
            lastMs = SystemClock.uptimeMillis();
        }
        postInvalidateOnAnimation();
    }

    /** Memajukan animasi satu frame. True bila masih perlu frame berikutnya. */
    private boolean step() {
        long now = SystemClock.uptimeMillis();
        float dt = Math.min((float) (now - lastMs), LIMIT_FRAME_MS);
        lastMs = now;
        boolean lanjut = false;

        if (drag) {
            dragProgress += dt / dragDuration;
            if (dragProgress >= 1f) {
                position = dragTarget;
                drag = false;
            } else {
                float t = dragBounce
                        ? bounce.getInterpolation(dragProgress)
                        : mengerem.getInterpolation(dragProgress);
                position = dragInitial + (dragTarget - dragInitial) * t;
                lanjut = true;
            }
        }

        if (press != pressTarget) {
            float maju = dt / DURATION_PRESS;
            press = press < pressTarget
                    ? Math.min(pressTarget, press + maju)
                    : Math.max(pressTarget, press - maju);
            if (press != pressTarget)
                lanjut = true;
        }
        return lanjut;
    }

    /** Menghentikan animasi dan mengembalikan tampilan ke status yang sebenarnya. */
    private void stopAnimation() {
        drag = false;
        running = false;
        press = 0f;
        pressTarget = 0f;
        position = isChecked() ? 1f : 0f;
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        stopAnimation();
    }

    @Override
    protected void onDetachedFromWindow() {
        // Kabar yang tertunda dikirim sekarang supaya tidak hilang, dan knob
        // tidak tertinggal setengah jalan.
        removeCallbacks(kabarkan);
        kabarkan.run();
        stopAnimation();
        super.onDetachedFromWindow();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        boolean lanjut = running && step();
        if (lanjut)
            postInvalidateOnAnimation();
        else
            running = false;

        float width = getWidth();
        float height = getHeight();
        float pillWidth = Math.min(width, pillWidthPx);
        float pillHeight = pillHeightPx;
        float line = linePx;

        float left = (width - pillWidth) / 2f;
        float top = (height - pillHeight) / 2f;
        float radius = pillHeight / 2f;

        // Warna mengikuti posisi tetapi dijepit supaya overshoot tidak membuat
        // warna keluar dari rentang. Theme.campur tidak membuat objek baru.
        float f = Math.max(0f, Math.min(1f, position));
        int trackColor = Theme.mix(Theme.SWITCH_OFF_TRACK, Theme.SWITCH_ON_TRACK, f);
        int knobColor = Theme.mix(Theme.SWITCH_OFF_KNOB, Theme.SWITCH_ON_KNOB, f);

        // Garis luar #B28950 sebagai pil penuh, lalu track di dalamnya. Saat mati
        // track memang berwarna sama dengan garis, persis seperti di game.
        cat.setStyle(Paint.Style.FILL);
        cat.setColor(Theme.STROKE_CONTROL);
        field.set(left, top, left + pillWidth, top + pillHeight);
        canvas.drawRoundRect(field, radius, radius, cat);

        cat.setColor(trackColor);
        field.set(left + line, top + line, left + pillWidth - line, top + pillHeight - line);
        float radiusInside = radius - line;
        canvas.drawRoundRect(field, radiusInside, radiusInside, cat);

        // Knob bulat mengisi penuh tinggi bagian dalam. Saat ditekan knob melar
        // ke arah tengah supaya tepi luarnya tidak lompat.
        float diameter = pillHeight - 2f * line;
        float stretch = stretchPx * press;
        float jalur = pillWidth - 2f * line - diameter;
        float knobLeft = left + line + jalur * position - stretch * position;
        field.set(knobLeft, top + line, knobLeft + diameter + stretch, top + line + diameter);

        cat.setColor(knobColor);
        canvas.drawRoundRect(field, diameter / 2f, diameter / 2f, cat);
    }
}
