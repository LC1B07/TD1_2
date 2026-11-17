
#include "bullet.h"

// 弾の更新
void UpdateBullets(GameState& state) {

    for (int i = 0; i < kMaxBullets; ++i) {

        Bullet& b = state.bullets[i];

        if (!b.active) {
            continue;
        }

        // 反射弾の残像
        if (b.reflected) {

            for (int j = kTrailLength - 1; j > 0; --j) {
                b.trail[j] = b.trail[j - 1];
            }

            b.trail[0] = { b.x, b.y };

            if (b.trailCount < kTrailLength) {
                ++b.trailCount;
            }
        }

        // 弾の移動
        b.x += b.vx;
        b.y += b.vy;

        // カウンター判定
        if (!b.reflected && state.justWindow) {

            float dx = b.x - (state.player.x + 18.0f);
            float dy = b.y - (state.player.y + 24.0f);

            if (dx * dx + dy * dy <= 90.0f * 90.0f) {

                const bool dashJustCounter = state.dashTimer > 0;

                state.counterEffect = {
                    b.x,
                    b.y,
                    30,
                    dashJustCounter
                };

                state.shakeTotalFrames = dashJustCounter
                    ? kDashCounterShakeFrames
                    : kCounterShakeFrames;

                state.shakeMaxPower = dashJustCounter
                    ? kDashCounterShakePower
                    : kCounterShakePower;

                state.shakeTimer = state.shakeTotalFrames;

                state.hitStopTimer = dashJustCounter
                    ? kDashCounterHitStopFrames
                    : kCounterHitStopFrames;

                state.slowTimer = dashJustCounter
                    ? kDashCounterSlowFrames
                    : kCounterSlowFrames;

                if (dashJustCounter) {
                    state.specialFlashTimer = kDashCounterFlashFrames;
                }

                state.gameFrameAccumulator = 0.0f;

                state.counterCooldown = kCounterRetryCooldown;

                float toBossX = state.boss.x + 50.0f - b.x;
                float toBossY = state.boss.y + 60.0f - b.y;

                float length = sqrtf(
                    toBossX * toBossX +
                    toBossY * toBossY
                );

                if (length < 0.001f) {
                    length = 1.0f;
                }

                const float reflectedSpeed = dashJustCounter
                    ? kDashCounterBulletSpeed
                    : kCounterBulletSpeed;

                b.vx = toBossX / length * reflectedSpeed;
                b.vy = toBossY / length * reflectedSpeed;

                b.reflected = true;
                b.dashReflected = dashJustCounter;

                b.trailCount = 0;

                state.counterWindowTimer = 0;

                state.swordSwingTimer = kSwordSwingFrames;

                state.normalSwordActive = false;
                state.counterArmed = false;
            }
        }

        // 画面外
        if (
            b.x < -20 || b.x > kWidth + 20 ||
            b.y < -20 || b.y > kHeight + 20
            ) {

            b.active = false;
            continue;
        }

        // ボスへの命中
        if (
            b.reflected &&
            IsHit(
                state.boss.x, state.boss.y, 100, 120,
                b.x - 8, b.y - 8, 16, 16
            )
            ) {

            b.active = false;

            state.bossFlash = 12;

            if (b.dashReflected) {

                state.specialImpactX = b.x;
                state.specialImpactY = b.y;

                state.specialImpactTimer = kDashCounterImpactFrames;
            }

            continue;
        }

        // プレイヤーへの命中
        if (
            !b.reflected &&
            IsHit(
                state.player.x, state.player.y, 36, 48,
                b.x - 8, b.y - 8, 16, 16
            )
            ) {

            b.active = false;
            continue;
        }
    }
}
