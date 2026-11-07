package com.android.support;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.content.Context;
import android.provider.Settings;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.DecelerateInterpolator;

import java.util.Map;
import java.util.WeakHashMap;

/**
 * Transisi singkat untuk menu: fade saat panel atau halaman berganti dan lipat
 * halus untuk section. Semua transisi dilewati bila pengguna mematikan animasi
 * sistem (skala animasi 0), dan view selalu berakhir di keadaan akhir yang benar.
 */
final class Transition {
    private static final long DURATION_MS = 160L;
    private static final float INITIAL_SCALE = 0.94f;

    private static final Map<View, ValueAnimator> lipatan = new WeakHashMap<View, ValueAnimator>();

    private Transition() {
    }

    /** False bila animasi dimatikan di pengaturan sistem. */
    static boolean active(Context context) {
        try {
            return Settings.Global.getFloat(context.getContentResolver(),
                    Settings.Global.ANIMATOR_DURATION_SCALE, 1f) > 0f;
        } catch (Throwable t) {
            return true;
        }
    }

    /** Memunculkan view dengan fade dari transparan sampai {@code alphaAkhir}. */
    static void muncul(View view, float alphaEnd) {
        view.animate().cancel();
        if (!active(view.getContext())) {
            view.setAlpha(alphaEnd);
            return;
        }
        view.setAlpha(0f);
        view.animate().alpha(alphaEnd).setDuration(DURATION_MS)
                .setInterpolator(new DecelerateInterpolator()).start();
    }

    /** Dialog muncul dengan fade dan sedikit membesar dari tengah. */
    static void enter(View view) {
        view.animate().cancel();
        if (!active(view.getContext())) {
            view.setAlpha(1f);
            view.setScaleX(1f);
            view.setScaleY(1f);
            return;
        }
        view.setAlpha(0f);
        view.setScaleX(INITIAL_SCALE);
        view.setScaleY(INITIAL_SCALE);
        view.animate().alpha(1f).scaleX(1f).scaleY(1f).setDuration(DURATION_MS)
                .setInterpolator(new DecelerateInterpolator()).start();
    }

    /** Membuka atau menutup isi section dengan animasi tinggi. */
    static void fold(final View content, final boolean open) {
        ValueAnimator old = lipatan.remove(content);
        if (old != null)
            old.cancel();

        final ViewGroup.LayoutParams params = content.getLayoutParams();
        int widthParent = content.getParent() instanceof View ? ((View) content.getParent()).getWidth() : 0;
        boolean canAnimation = params != null && widthParent > 0 && content.getWindowToken() != null
                && active(content.getContext());
        if (!canAnimation) {
            if (params != null)
                params.height = ViewGroup.LayoutParams.WRAP_CONTENT;
            content.setVisibility(open ? View.VISIBLE : View.GONE);
            content.requestLayout();
            return;
        }

        int initial;
        int end;
        if (open) {
            content.measure(View.MeasureSpec.makeMeasureSpec(widthParent, View.MeasureSpec.EXACTLY),
                    View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));
            initial = content.getVisibility() == View.VISIBLE ? content.getHeight() : 0;
            end = content.getMeasuredHeight();
            params.height = initial;
            content.setVisibility(View.VISIBLE);
        } else {
            initial = content.getHeight();
            end = 0;
        }

        final boolean[] batal = {false};
        ValueAnimator animator = ValueAnimator.ofInt(initial, end);
        animator.setDuration(DURATION_MS);
        animator.setInterpolator(new DecelerateInterpolator());
        animator.addUpdateListener(new ValueAnimator.AnimatorUpdateListener() {
            @Override
            public void onAnimationUpdate(ValueAnimator animation) {
                params.height = (Integer) animation.getAnimatedValue();
                content.setLayoutParams(params);
            }
        });
        animator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationCancel(Animator animation) {
                batal[0] = true;
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                if (batal[0])
                    return;
                lipatan.remove(content);
                params.height = ViewGroup.LayoutParams.WRAP_CONTENT;
                content.setVisibility(open ? View.VISIBLE : View.GONE);
                content.setLayoutParams(params);
            }
        });
        lipatan.put(content, animator);
        animator.start();
    }
}
