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
 * Short transitions for the menu: fade when a panel or page changes and a smooth
 * fold for sections. All transitions are skipped when the user turns off system
 * animations (animation scale 0), and the view always ends in the correct final state.
 */
final class Transition {
    private static final long DURATION_MS = 160L;
    private static final float INITIAL_SCALE = 0.94f;

    private static final Map<View, ValueAnimator> folds = new WeakHashMap<View, ValueAnimator>();

    private Transition() {
    }

    /** False when animations are turned off in the system settings. */
    static boolean active(Context context) {
        try {
            return Settings.Global.getFloat(context.getContentResolver(),
                    Settings.Global.ANIMATOR_DURATION_SCALE, 1f) > 0f;
        } catch (Throwable t) {
            return true;
        }
    }

    /** Reveals the view with a fade from transparent to {@code alphaEnd}. */
    static void appear(View view, float alphaEnd) {
        view.animate().cancel();
        if (!active(view.getContext())) {
            view.setAlpha(alphaEnd);
            return;
        }
        view.setAlpha(0f);
        view.animate().alpha(alphaEnd).setDuration(DURATION_MS)
                .setInterpolator(new DecelerateInterpolator()).start();
    }

    /** The dialog appears with a fade and a slight scale up from the center. */
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

    /** Opens or closes the section content with a height animation. */
    static void fold(final View content, final boolean open) {
        ValueAnimator old = folds.remove(content);
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

        final boolean[] cancelled = {false};
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
                cancelled[0] = true;
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                if (cancelled[0])
                    return;
                folds.remove(content);
                params.height = ViewGroup.LayoutParams.WRAP_CONTENT;
                content.setVisibility(open ? View.VISIBLE : View.GONE);
                content.setLayoutParams(params);
            }
        });
        folds.put(content, animator);
        animator.start();
    }
}
