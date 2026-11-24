#include "BossB.h"
#include <cmath>
#include <cstring>

namespace BossB {
namespace {
    // game.h 側の画面定数に依存しないよう、このボス用の画面範囲を定義。
    constexpr float kScreenWidth = 1920.0f;
    constexpr float kScreenHeight = 1080.0f;

    constexpr float kGravity = 0.55f;
    constexpr float kJumpPower = -12.0f;
    constexpr float kMoveSpeed = 5.0f;
    constexpr float kDashSpeed = 16.0f;
    constexpr int kDashFrames = 10;
    constexpr int kDashCooldownFrames = 30;
    constexpr int kSwordFrames = 16;
    constexpr int kCounterWindow = 8;
    constexpr int kCounterCooldownFrames = 30;

    constexpr int kWireInterval = 300;
    constexpr int kWireWarningFrames = 90;
    constexpr float kWireSpeed = 16.0f;
    constexpr int kWireBoundFrames = 125;

    constexpr int kBreathInterval = 420;
    constexpr int kBreathWarningFrames = 75;
    constexpr int kBreathExtendFrames = 28;
    constexpr int kBreathHoldFrames = 65;
    constexpr int kBreathRecoveryFrames = 45;
    constexpr float kBreathRadius = 45.0f;

    constexpr int kChargeWarningFrames = 85;
    constexpr int kChargeRecoveryFrames = 45;
    constexpr int kChargeDownFrames = 240;
    constexpr int kChargeGetUpFrames = 35;
    constexpr float kChargeSpeed = 32.0f;
    constexpr float kChargeWidth = 230.0f;
    constexpr float kChargeHeight = 110.0f;
    constexpr float kChargeFallGravity = 1.25f;

    constexpr float kDragonX = 1490.0f;
    constexpr float kDragonY = 0.0f;
    constexpr float kMouthX = 25.0f;
    constexpr float kMouthY = 115.0f;

    bool IsHit(float ax, float ay, float aw, float ah,
        float bx, float by, float bw, float bh) {
        return ax < bx + bw && ax + aw > bx &&
            ay < by + bh && ay + ah > by;
    }

    unsigned int WithAlpha(unsigned int color, int alpha) {
        if (alpha < 0) alpha = 0;
        if (alpha > 255) alpha = 255;
        return (color & 0xFFFFFF00u) | static_cast<unsigned int>(alpha);
    }

    bool IsBusy(const State& s) {
        return s.wire.phase != WirePhase::Idle ||
            s.breath.phase != BreathPhase::Idle ||
            s.charge.phase != ChargePhase::Idle;
    }

    void UpdateCharge(State& s) {
        DragonCharge& c = s.charge;

        switch (c.phase) {
        case ChargePhase::Warning:
            if (++c.timer >= kChargeWarningFrames) {
                c.phase = ChargePhase::Rush;
                c.timer = 0;
                c.x = 0.0f;
                c.y = kGroundY - kChargeHeight;
                c.vx = kChargeSpeed;
                c.vy = 0.0f;
            }
            break;

        case ChargePhase::Leave:
            c.phase = ChargePhase::Rush;
            c.timer = 0;
            break;

        case ChargePhase::Rush:
            c.x += c.vx;
            c.y += c.vy;
            if (c.x > kScreenWidth || c.x + kChargeWidth < 0.0f) {
                c.phase = ChargePhase::Fall;
                c.timer = 0;
                c.fallVelocityY = 0.0f;
            }
            break;

        case ChargePhase::Fall:
            c.fallVelocityY += kChargeFallGravity;
            c.y += c.fallVelocityY;
            c.fallSpin += 0.08f;
            if (c.y + kChargeHeight >= kGroundY) {
                c.y = kGroundY - kChargeHeight;
                c.phase = ChargePhase::Down;
                c.timer = 0;
                c.dustTimer = 36;
            }
            break;

        case ChargePhase::Down:
            if (++c.timer >= kChargeDownFrames) {
                c.phase = ChargePhase::GetUp;
                c.timer = 0;
            }
            break;

        case ChargePhase::GetUp:
            if (++c.timer >= kChargeGetUpFrames) {
                c.phase = ChargePhase::Recovery;
                c.timer = 0;
            }
            break;

        case ChargePhase::Recovery:
            if (++c.timer >= kChargeRecoveryFrames) {
                c.phase = ChargePhase::Idle;
                c.timer = 0;
            }
            break;

        default:
            break;
        }

        if (c.hitFlash > 0) --c.hitFlash;
        if (c.dustTimer > 0) --c.dustTimer;
    }

    void UpdateBreath(State& s) {
        DragonBreath& b = s.breath;
        if (b.hitFlash > 0) --b.hitFlash;
        if (b.hitCooldown > 0) --b.hitCooldown;

        switch (b.phase) {
        case BreathPhase::Warning:
            if (++b.timer >= kBreathWarningFrames) {
                b.phase = BreathPhase::Extend;
                b.timer = 0;
            }
            break;
        case BreathPhase::Extend:
            ++b.timer;
            b.length = kScreenWidth * static_cast<float>(b.timer) / kBreathExtendFrames;
            if (b.timer >= kBreathExtendFrames) {
                b.phase = BreathPhase::Hold;
                b.timer = 0;
                b.length = kScreenWidth;
            }
            break;
        case BreathPhase::Hold:
            if (++b.timer >= kBreathHoldFrames) {
                b.phase = BreathPhase::Recovery;
                b.timer = 0;
            }
            break;
        case BreathPhase::Recovery:
            if (++b.timer >= kBreathRecoveryFrames) {
                b.phase = BreathPhase::Idle;
                b.timer = 0;
                b.length = 0.0f;
            }
            break;
        default:
            break;
        }

        if ((b.phase == BreathPhase::Extend || b.phase == BreathPhase::Hold) &&
            b.hitCooldown == 0 && b.length > 0.0f) {
            const float px = s.player.x + 18.0f - b.startX;
            const float py = s.player.y + 24.0f - b.startY;
            float along = px * b.dirX + py * b.dirY;
            if (along < 0.0f) along = 0.0f;
            if (along > b.length) along = b.length;

            const float nearX = b.startX + b.dirX * along;
            const float nearY = b.startY + b.dirY * along;
            const float dx = s.player.x + 18.0f - nearX;
            const float dy = s.player.y + 24.0f - nearY;
            const float radius = kBreathRadius + 22.0f;

            if (dx * dx + dy * dy <= radius * radius) {
                if (s.player.dashTimer > 0) s.dashDodgeEffect = 20;
                else {
                    b.hitFlash = 14;
                    s.player.hitFlash = 14;
                }
                b.hitCooldown = s.player.dashTimer > 0 ? 12 : 45;
            }
        }
    }

    void UpdateWire(State& s, const char keys[], const char preKeys[]) {
        WireAttack& w = s.wire;
        const bool justJ = keys[DIK_J] && !preKeys[DIK_J];

        if (w.phase == WirePhase::Warning) {
            if (++w.timer >= kWireWarningFrames) {
                w.phase = WirePhase::Flying;
                w.timer = 0;
                w.x = w.groundX;
                w.y = w.groundY;
                w.targetX = s.player.x + 18.0f;
                w.targetY = s.player.y + 24.0f;

                const float dx = w.targetX - w.x;
                const float dy = w.targetY - w.y;
                float len = std::sqrt(dx * dx + dy * dy);
                if (len < 0.001f) len = 1.0f;
                w.vx = dx / len * kWireSpeed;
                w.vy = dy / len * kWireSpeed;
            }
        } else if (w.phase == WirePhase::Flying) {
            ++w.timer;
            bool caught = false;

            // 1フレームを4分割して移動させ、すり抜けを軽減する。
            for (int i = 0; i < 4; ++i) {
                w.x += w.vx * 0.25f;
                w.y += w.vy * 0.25f;
                if (IsHit(s.player.x, s.player.y, kPlayerWidth, kPlayerHeight,
                    w.x - 11.0f, w.y - 11.0f, 22.0f, 22.0f)) {
                    caught = true;
                }
            }

            if (caught) {
                if (s.player.counterTimer > 0 || justJ) {
                    w.phase = WirePhase::Recovery;
                    w.timer = 0;
                    w.escaped = true;
                    s.wireEscapeEffect = 30;
                } else if (s.player.dashTimer > 0) {
                    w.phase = WirePhase::Recovery;
                    w.timer = 0;
                    w.escaped = true;
                    s.dashDodgeEffect = 20;
                } else {
                    w.phase = WirePhase::Bound;
                    w.timer = 0;
                    s.player.hitFlash = 15;
                }
            }

            if (w.timer > 100) {
                w.phase = WirePhase::Recovery;
                w.timer = 0;
            }
        } else if (w.phase == WirePhase::Bound) {
            if (justJ || s.player.counterTimer > 0) {
                w.phase = WirePhase::Recovery;
                w.timer = 0;
                w.escaped = true;
                s.wireEscapeEffect = 30;
            } else if (++w.timer >= kWireBoundFrames) {
                w.phase = WirePhase::Recovery;
                w.timer = 0;
            }
        } else if (w.phase == WirePhase::Recovery) {
            if (++w.timer >= 45) {
                w.phase = WirePhase::Idle;
                w.timer = 0;
            }
        }
    }

    void UpdateBullets(State& s) {
        for (auto& b : s.bullets) {
            if (!b.active) continue;

            b.x += b.vx;
            b.y += b.vy;

            if (b.x < -30.0f || b.x > kScreenWidth + 30.0f ||
                b.y < -30.0f || b.y > kScreenHeight + 30.0f) {
                b.active = false;
                continue;
            }

            if (IsHit(s.player.x, s.player.y, kPlayerWidth, kPlayerHeight,
                b.x - 8.0f, b.y - 8.0f, 16.0f, 16.0f)) {
                if (s.player.counterTimer > 0) {
                    const float dx = (kDragonX + 200.0f) - b.x;
                    const float dy = 360.0f - b.y;
                    float len = std::sqrt(dx * dx + dy * dy);
                    if (len < 0.001f) len = 1.0f;
                    b.vx = dx / len * 23.0f;
                    b.vy = dy / len * 23.0f;
                } else if (s.player.dashTimer > 0) {
                    b.active = false;
                    s.dashDodgeEffect = 16;
                } else {
                    b.active = false;
                    s.player.hitFlash = 18;
                }
            }

            // カウンターで反射した弾が竜の胴体に戻ったかを判定。
            if (b.active && b.vx < 0.0f &&
                b.x < kDragonX + 450.0f && b.x > kDragonX + 150.0f &&
                b.y > 0.0f && b.y < kGroundY) {
                s.bossHitFlash = 10;
                b.active = false;
            }
        }
    }

    void FireBullet(State& s) {
        for (auto& b : s.bullets) {
            if (b.active) continue;

            const float startX = kDragonX + 12.0f;
            const float startY = 360.0f;
            const float dx = s.player.x + 18.0f - startX;
            const float dy = s.player.y + 24.0f - startY;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len < 0.001f) len = 1.0f;

            b.x = startX;
            b.y = startY;
            b.vx = dx / len * 7.0f;
            b.vy = dy / len * 7.0f;
            b.active = true;
            break;
        }
    }

    void DrawWire(const WireAttack& w, int sx, int sy) {
        if (w.phase == WirePhase::Warning) {
            Novice::ScreenPrintf(520, 160, "WIRE WARNING!");
        } else if (w.phase == WirePhase::Flying || w.phase == WirePhase::Bound) {
            Novice::DrawEllipse((int)w.x + sx, (int)w.y + sy,
                11, 11, 0.0f, 0xD8D8D8FF, kFillModeSolid);
            if (w.phase == WirePhase::Bound) {
                Novice::ScreenPrintf(520, 185, "BOUND! PRESS J");
            }
        }
    }

    void DrawBreath(const DragonBreath& b, int sx, int sy) {
        if (b.phase == BreathPhase::Warning) {
            Novice::ScreenPrintf(520, 210, "BREATH WARNING!");
        }

        if (b.phase == BreathPhase::Extend || b.phase == BreathPhase::Hold) {
            const int endX = (int)(b.startX + b.dirX * b.length) + sx;
            const int endY = (int)(b.startY + b.dirY * b.length) + sy;
            Novice::DrawLine((int)b.startX + sx, (int)b.startY + sy,
                endX, endY, b.hitFlash > 0 ? 0xFFFFFFFF : 0xFF6A36FF);
        }
    }
} // namespace

void Reset(State& state) {
    state = State{};
    state.player.x = 210.0f;
    state.player.y = kGroundY - kPlayerHeight;
    state.player.direction = 1;
    state.attackWait = 75;
    state.bossTexture = -1;
}

void Update(State& s, Scene& currentScene,
    const char keys[], const char preKeys[],
    bool leftPressed, bool rightPressed) {
    (void)currentScene; // このボス処理ではシーン遷移を直接行わない。

    Player& p = s.player;

    if (p.hitFlash > 0) --p.hitFlash;
    if (p.dashTimer > 0) --p.dashTimer;
    if (p.dashCooldown > 0) --p.dashCooldown;
    if (p.swordTimer > 0) --p.swordTimer;
    if (p.counterTimer > 0) --p.counterTimer;
    if (p.counterCooldown > 0) --p.counterCooldown;
    if (s.dashDodgeEffect > 0) --s.dashDodgeEffect;
    if (s.wireEscapeEffect > 0) --s.wireEscapeEffect;
    if (s.dashWaveTimer > 0) --s.dashWaveTimer;
    if (s.bossHitFlash > 0) --s.bossHitFlash;
    if (s.swordHitFlash > 0) --s.swordHitFlash;

    // 左右移動。引数の押下情報を優先し、キー配列でも操作できるようにする。
    const bool moveLeft = leftPressed || keys[DIK_A];
    const bool moveRight = rightPressed || keys[DIK_D];

    if (p.dashTimer == 0) {
        if (moveLeft) {
            p.x -= kMoveSpeed;
            p.direction = -1;
        }
        if (moveRight) {
            p.x += kMoveSpeed;
            p.direction = 1;
        }
    }

    if (p.x < 0.0f) p.x = 0.0f;
    if (p.x > kScreenWidth - kPlayerWidth) p.x = kScreenWidth - kPlayerWidth;

    // ジャンプはキーを押した瞬間だけ受け付ける。
    if (keys[DIK_SPACE] && !preKeys[DIK_SPACE] && p.onGround) {
        p.velocityY = kJumpPower;
        p.onGround = false;
    }

    p.velocityY += kGravity;
    p.y += p.velocityY;
    if (p.y + kPlayerHeight >= kGroundY) {
        p.y = kGroundY - kPlayerHeight;
        p.velocityY = 0.0f;
        p.onGround = true;
    }

    // ダッシュ
    if (keys[DIK_LSHIFT] && !preKeys[DIK_LSHIFT] && p.dashCooldown == 0) {
        p.dashTimer = kDashFrames;
        p.dashCooldown = kDashCooldownFrames;
        p.velocityY = 0.0f;
        s.dashWaveTimer = 18;
        s.dashWaveX = p.x + kPlayerWidth * 0.5f;
        s.dashWaveY = p.y + kPlayerHeight * 0.5f;
    }
    if (p.dashTimer > 0) {
        p.x += p.direction * kDashSpeed;
        if (p.x < 0.0f) p.x = 0.0f;
        if (p.x > kScreenWidth - kPlayerWidth) p.x = kScreenWidth - kPlayerWidth;
    }

    // 剣攻撃
    if (keys[DIK_H] && !preKeys[DIK_H]) {
        p.swordTimer = kSwordFrames;
    }

    // ジャストカウンター
    if (keys[DIK_J] && !preKeys[DIK_J] && p.counterCooldown == 0) {
        p.counterTimer = kCounterWindow;
        p.counterCooldown = kCounterCooldownFrames;
    }

    // 攻撃の開始。各攻撃が同時に始まらないよう IsBusy で管理する。
    if (s.attackWait > 0) {
        --s.attackWait;
    } else if (!IsBusy(s)) {
        const int nextAttack = (s.lastAttack + 1) % 3;
        s.lastAttack = nextAttack;

        if (nextAttack == 0) {
            s.wire.phase = WirePhase::Warning;
            s.wire.timer = 0;
            s.wire.groundX = kDragonX + 100.0f;
            s.wire.groundY = kGroundY - 20.0f;
        } else if (nextAttack == 1) {
            s.breath.phase = BreathPhase::Warning;
            s.breath.timer = 0;
            s.breath.startX = kDragonX + kMouthX;
            s.breath.startY = kDragonY + kMouthY + 360.0f;
            const float dx = (p.x + 18.0f) - s.breath.startX;
            const float dy = (p.y + 24.0f) - s.breath.startY;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len < 0.001f) len = 1.0f;
            s.breath.dirX = dx / len;
            s.breath.dirY = dy / len;
            s.breath.length = 0.0f;
        } else {
            s.charge.phase = ChargePhase::Warning;
            s.charge.timer = 0;
            s.charge.x = kDragonX;
            s.charge.y = kGroundY - kChargeHeight;
            s.charge.vx = -kChargeSpeed;
            s.charge.vy = 0.0f;
        }
        s.attackWait = 180;
    }

    UpdateWire(s, keys, preKeys);
    UpdateBreath(s);
    UpdateCharge(s);
    UpdateBullets(s);

    // 一定間隔で竜が弾を発射する。
    static int bulletTimer = 0;
    if (++bulletTimer >= 100) {
        FireBullet(s);
        bulletTimer = 0;
    }

    // 待機アニメーションの4コマを一定間隔で切り替える。
    if (++s.idleFrameTimer >= 12) {
        s.idleFrameTimer = 0;
        s.idleFrame = (s.idleFrame + 1) % 4;
    }

    // 突進パリィ後のダウン中だけ剣攻撃をボスに当てられる。
    if (p.swordTimer > 3 && s.charge.phase == ChargePhase::Down) {
        const float hitX = p.direction > 0 ? p.x + 18.0f : p.x - 82.0f;
        if (IsHit(hitX, p.y - 26.0f, 100.0f, 100.0f,
            s.charge.x, s.charge.y, kChargeWidth, kChargeHeight)) {
            s.bossHitFlash = 12;
            s.swordHitFlash = 12;
        }
    }
}

void Draw(const State& s) {
    const int sx = s.shakeX;
    const int sy = s.shakeY;

    Novice::ScreenPrintf(550, 60, "DRAGON BOSS BATTLE");
    Novice::DrawBox(0, (int)kGroundY + sy, (int)kScreenWidth, 4,
        0.0f, 0x53627CFF, kFillModeSolid);

    for (const auto& a : s.afterimages) {
        if (a.timer > 0) {
            Novice::DrawBox((int)a.x + sx, (int)a.y + sy, 36, 48, 0.0f,
                WithAlpha(0x4BBFFF00u, a.timer * 9), kFillModeSolid);
        }
    }

    if (s.dashWaveTimer > 0) {
        const int elapsed = 18 - s.dashWaveTimer;
        const int radius = 12 + elapsed * 7;
        Novice::DrawEllipse((int)s.dashWaveX + sx, (int)s.dashWaveY + sy,
            radius, radius, 0.0f,
            WithAlpha(0x7DDFFF00u, s.dashWaveTimer * 190 / 18),
            kFillModeWireFrame);
    }

    if (s.charge.phase != ChargePhase::Leave &&
        s.charge.phase != ChargePhase::Rush &&
        s.charge.phase != ChargePhase::Fall &&
        s.charge.phase != ChargePhase::Down &&
        s.charge.phase != ChargePhase::GetUp) {
        if (s.bossTexture >= 0) {
            Novice::DrawSpriteRect((int)kDragonX + sx, (int)kDragonY + sy,
                s.idleFrame * 1920, 0, 1920, 1080,
                s.bossTexture, 1.0f, 1.0f, 0.0f, 0xFFFFFFFF);
        } else {
            Novice::DrawBox((int)kDragonX + sx, (int)kDragonY + sy,
                430, 1080, 0.0f, 0xAD3D4EFF, kFillModeSolid);
        }
    }

    const DragonCharge& c = s.charge;
    if (c.phase == ChargePhase::Warning) {
        Novice::ScreenPrintf(500, 110, "DRAGON CHARGE WARNING!");
    }

    if (c.phase == ChargePhase::Rush || c.phase == ChargePhase::Fall ||
        c.phase == ChargePhase::Down || c.phase == ChargePhase::GetUp) {
        const int x = (int)c.x + sx;
        const int y = (int)c.y + sy;
        float rotation = c.phase == ChargePhase::Fall ? c.fallSpin : 0.0f;
        if (c.phase == ChargePhase::GetUp) {
            rotation = 0.06f * sinf((float)c.timer * 0.35f);
        }

        if (c.phase == ChargePhase::Rush) {
            for (int i = 3; i >= 1; --i) {
                Novice::DrawBox(
                    x - (int)(c.vx / kChargeSpeed * i * 40.0f),
                    y + 12 - (int)(c.vy / kChargeSpeed * i * 40.0f),
                    (int)kChargeWidth, (int)kChargeHeight - 24,
                    c.angle, WithAlpha(0xB84C4C00u, 55 - i * 12),
                    kFillModeSolid);
            }
        }

        const unsigned int color = c.hitFlash > 0 ? 0xFFFFFFFF : 0xAD3D4EFF;
        Novice::DrawBox(x, y, (int)kChargeWidth, (int)kChargeHeight,
            rotation, color, kFillModeSolid);
        Novice::DrawBox(x + 12, y + 14, (int)kChargeWidth - 24,
            (int)kChargeHeight - 28, rotation, 0xED7780FF, kFillModeSolid);

        if (c.phase == ChargePhase::Down) {
            Novice::ScreenPrintf(x + 65, y - 28, "DOWN! %d",
                (kChargeDownFrames - c.timer + 59) / 60);
        }

        if (c.dustTimer > 0) {
            const float elapsed = (float)(36 - c.dustTimer);
            for (int i = 0; i < 16; ++i) {
                const float side = (i % 2 == 0) ? -1.0f : 1.0f;
                const float px = c.x + kChargeWidth * 0.5f +
                    side * (20.0f + elapsed * (2.0f + (i % 5)));
                const float py = kGroundY - 12.0f -
                    sinf((float)i * 1.7f) * 13.0f -
                    elapsed * (0.5f + (i % 3) * 0.3f);
                const int r = 7 + i % 6 + (int)(elapsed * 0.3f);
                Novice::DrawEllipse((int)px + sx, (int)py + sy,
                    r, r / 2 + 3, 0.0f,
                    WithAlpha(0xB6A69A00, c.dustTimer * 5),
                    kFillModeSolid);
            }
        }
    }

    DrawWire(s.wire, sx, sy);
    DrawBreath(s.breath, sx, sy);

    for (const auto& b : s.bullets) {
        if (b.active) {
            Novice::DrawEllipse((int)b.x + sx, (int)b.y + sy,
                8, 8, 0.0f, 0xFFD16AFF, kFillModeSolid);
        }
    }

    if (s.breath.hitFlash > 0) {
        Novice::ScreenPrintf(475, 215, "BREATH HIT!");
    }
    if (s.dashDodgeEffect > 0) {
        Novice::ScreenPrintf(520, 240, "DASH DODGE!");
        Novice::DrawEllipse((int)s.player.x + 18 + sx,
            (int)s.player.y + 24 + sy, 40, 48, 0.0f,
            WithAlpha(0x78EFFFFF, s.dashDodgeEffect * 11),
            kFillModeWireFrame);
    }

    // プレイヤーと剣
    Novice::DrawBox((int)s.player.x + sx, (int)s.player.y + sy,
        (int)kPlayerWidth, (int)kPlayerHeight, 0.0f,
        s.player.hitFlash > 0 ? 0xFFFFFFFF : 0x66BFFFFF,
        kFillModeSolid);

    const float cx = s.player.x + 18.0f +
        s.player.direction * 12.0f + sx;
    const float cy = s.player.y + 24.0f + sy;
    float angle = s.player.direction > 0 ? -0.69f : 3.14159265f + 0.69f;

    if (s.player.swordTimer > 0) {
        angle += (s.player.direction > 0 ? 1.8f : -1.8f) *
            (1.0f - (float)s.player.swordTimer / kSwordFrames);
    }

    const float tipX = cx + cosf(angle) * 58.0f;
    const float tipY = cy + sinf(angle) * 58.0f;

    Novice::DrawLine((int)cx, (int)cy, (int)tipX, (int)tipY, 0x52BFFFFF);
    Novice::DrawLine(
        (int)(cx - sinf(angle) * 2),
        (int)(cy + cosf(angle) * 2),
        (int)(tipX - sinf(angle) * 2),
        (int)(tipY + cosf(angle) * 2),
        0xE5F9FFFF);

    if (s.player.counterTimer > 0) {
        Novice::ScreenPrintf(520, 270, "JUST COUNTER WINDOW");
    }

    Novice::ScreenPrintf(30, 30,
        "A/D : MOVE  SPACE : JUMP  SHIFT : DASH  H : SWORD  J : COUNTER");
    Novice::ScreenPrintf(30, 55,
        "WIRE: J BEFORE THE TAIL TOUCHES YOU | DASH: AVOID BREATH");

    if (s.player.dashCooldown > 0) {
        Novice::ScreenPrintf(30, 130, "DASH COOLDOWN: %d",
            s.player.dashCooldown);
    } else {
        Novice::ScreenPrintf(30, 130, "DASH READY");
    }

    if (s.swordHitFlash > 0) {
        Novice::ScreenPrintf(560, 190, "SWORD HIT!");
    }
}

} // namespace BossB
