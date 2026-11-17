
#include "../Effect/effect.h"
#include <cmath>

// 演出タイマー更新
void UpdateEffectTimers(GameState& state) {

    if (state.dashCooldown > 0) {
        --state.dashCooldown;
    }

    if (state.dashWave.timer > 0) {
        --state.dashWave.timer;
    }

    for (int i = 0; i < kDashAfterimageCount; ++i) {

        if (state.dashImages[i].timer > 0) {
            --state.dashImages[i].timer;
        }
    }

    if (state.counterCooldown > 0) {
        --state.counterCooldown;
    }

    if (state.counterEffect.timer > 0) {
        --state.counterEffect.timer;
    }

    if (state.bossFlash > 0) {
        --state.bossFlash;
    }

    if (state.swordHitFlashTimer > 0) {
        --state.swordHitFlashTimer;
    }

    if (state.shakeTimer > 0) {
        --state.shakeTimer;
    }

    if (state.specialFlashTimer > 0) {
        --state.specialFlashTimer;
    }

    if (state.specialImpactTimer > 0) {
        --state.specialImpactTimer;
    }
}

// カウンターエフェクト描画
void DrawCounterEffect(
    const CounterEffect& e,
    int shakeX,
    int shakeY
) {

    if (e.timer <= 0) {
        return;
    }

    const int elapsed = 30 - e.timer;
    const float t = (float)e.timer / 30.0f;

    const int cx = (int)e.x + shakeX;
    const int cy = (int)e.y + shakeY;

    // 2重の衝撃波
    for (int i = 0; i < 2; ++i) {

        int radius = 12 + elapsed * 5 - i * 18;

        if (radius <= 0) {
            continue;
        }

        unsigned int color =
            WithAlpha(0x72DFFFFFu, (int)(190.0f * t));

        Novice::DrawEllipse(
            cx, cy,
            radius, radius,
            0.0f,
            color,
            kFillModeWireFrame
        );
    }

    // 強化カウンター
    if (e.enhanced) {

        for (int i = 0; i < 3; ++i) {

            int radius = 30 + elapsed * 12 - i * 28;

            if (radius > 0) {

                Novice::DrawEllipse(
                    cx, cy,
                    radius, radius,
                    0.0f,
                    WithAlpha(
                        0xD7FFFFFFu,
                        (int)(225.0f * t) - i * 25
                    ),
                    kFillModeWireFrame
                );
            }
        }

        // 放射状の光線
        for (int i = 0; i < 12; ++i) {

            float a = 6.2831853f * i / 12.0f;

            float dx = cosf(a);
            float dy = sinf(a);

            float inner = 24.0f + elapsed * 7.0f;
            float outer = inner + 80.0f;

            Novice::DrawLine(
                (int)(cx + dx * inner),
                (int)(cy + dy * inner),
                (int)(cx + dx * outer),
                (int)(cy + dy * outer),
                WithAlpha(0xA4F2FFFFu, (int)(200.0f * t))
            );
        }
    }

    // 十字閃光
    if (elapsed < 7) {

        int len = 65 - elapsed * 6;

        unsigned int flash =
            WithAlpha(0xFFFFFFFFu, 255 - elapsed * 30);

        Novice::DrawLine(
            cx - len, cy,
            cx + len, cy,
            flash
        );

        Novice::DrawLine(
            cx, cy - len,
            cx, cy + len,
            flash
        );

        Novice::DrawEllipse(
            cx, cy,
            15, 15,
            0.0f,
            WithAlpha(0xA8FFFFFFu, 170 - elapsed * 20),
            kFillModeSolid
        );
    }

    // 放射状の青いスパーク
    const float pi = 3.14159265f;

    for (int i = 0; i < 16; ++i) {

        float angle =
            (2.0f * pi * i / 16.0f) + (i % 3) * 0.15f;

        float dist =
            10.0f + elapsed * (2.0f + (i % 4) * 0.6f);

        float length = 8.0f + (i % 4) * 5.0f;

        float dx = cosf(angle);
        float dy = sinf(angle);

        Novice::DrawLine(
            (int)(e.x + dx * dist) + shakeX,
            (int)(e.y + dy * dist) + shakeY,
            (int)(e.x + dx * (dist + length)) + shakeX,
            (int)(e.y + dy * (dist + length)) + shakeY,
            WithAlpha(0x70CFFFFFu, (int)(235.0f * t))
        );
    }
}
