#pragma once

#include <Novice.h>
#include "../../Game/game.h"

// 竜ボスB専用の処理・データ。
// 共通の Player / Bullet などと名前が衝突しないよう BossB 名前空間にまとめる。
namespace BossB {

    constexpr float kGroundY = 930.0f;
    constexpr float kPlayerWidth = 36.0f;
    constexpr float kPlayerHeight = 48.0f;

    enum class WirePhase { Idle, Warning, Flying, Bound, Recovery };
    enum class BreathPhase { Idle, Warning, Extend, Hold, Recovery };
    enum class ChargePhase { Idle, Warning, Leave, Rush, Fall, Down, GetUp, Recovery };

    struct Player {
        float x = 210.0f;
        float y = kGroundY - kPlayerHeight;
        float velocityY = 0.0f;
        int direction = 1;
        bool onGround = true;
        int dashTimer = 0;
        int dashCooldown = 0;
        int swordTimer = 0;
        int counterTimer = 0;
        int counterCooldown = 0;
        int hitFlash = 0;
    };

    struct WireAttack {
        WirePhase phase = WirePhase::Idle;
        int timer = 0;
        float x = 0.0f, y = 0.0f;
        float vx = 0.0f, vy = 0.0f;
        float groundX = 0.0f, groundY = 0.0f;
        float targetX = 0.0f, targetY = 0.0f;
        bool escaped = false;
    };

    struct DragonBreath {
        BreathPhase phase = BreathPhase::Idle;
        int timer = 0;
        int hitCooldown = 0;
        int hitFlash = 0;
        float startX = 0.0f, startY = 0.0f;
        float dirX = 0.0f, dirY = 0.0f;
        float length = 0.0f;
    };

    struct DragonCharge {
        ChargePhase phase = ChargePhase::Idle;
        int timer = 0;
        float x = 0.0f, y = 0.0f;
        float vx = 0.0f, vy = 0.0f;
        float targetY = 0.0f;
        float angle = 0.0f;
        float fallVelocityY = 0.0f;
        float fallSpin = 0.0f;
        int hitFlash = 0;
        int dustTimer = 0;
        bool hitOnce = false;
    };

    struct Bullet {
        float x = 0.0f, y = 0.0f;
        float vx = 0.0f, vy = 0.0f;
        bool active = false;
    };

    constexpr int kBulletMax = 20;
    constexpr int kAfterimageMax = 8;

    struct Afterimage {
        float x = 0.0f, y = 0.0f;
        int timer = 0;
    };

    struct State {
        Player player;
        WireAttack wire;
        DragonBreath breath;
        DragonCharge charge;
        Bullet bullets[kBulletMax]{};
        Afterimage afterimages[kAfterimageMax]{};

        int afterimageIndex = 0;
        int dashWaveTimer = 0;
        float dashWaveX = 0.0f, dashWaveY = 0.0f;
        int attackWait = 75;
        int lastAttack = -1;
        int wireEscapeEffect = 0;
        int dashDodgeEffect = 0;
        int shakeTimer = 0;
        int shakeX = 0, shakeY = 0;
        int idleFrame = 0;
        int idleFrameTimer = 0;
        int bossTexture = -1;
        bool swordHit = false;
        int swordHitFlash = 0;
        int bossHitFlash = 0;
    };

    // main.cpp から呼び出す関数
    void Reset(State& state);
    void Update(State& state, Scene& currentScene,
        const char keys[], const char preKeys[],
        bool leftPressed, bool rightPressed);
    void Draw(const State& state);

} // namespace BossB
