
#pragma once

#include <Novice.h>
#include <cmath>
#include <cstring>

// シーンの種類
enum Scene {
    SCENE_TITLE,
    SCENE_BOSS,
    SCENE_GAMEOVER,
    SCENE_CLEAR,
    SCENE_CREDITS
};

// 画面サイズ
const int kWidth = 1280;
const int kHeight = 720;

// ボス戦の設定
const float kGroundY = 620.0f;
const float kGravity = 0.55f;
const float kJumpPower = -12.0f;
const float kMoveSpeed = 5.0f;

// ダッシュ
const float kDashSpeed = 16.0f;
const int kDashDuration = 10;
const int kDashCooldown = 30;
const int kDashWaveFrames = 18;
const int kDashAfterimageCount = 8;

// 剣
const int kSwordSwingFrames = 16;
const float kSwordLength = 58.0f;
const float kSwordHitWidth = 100.0f;
const float kSwordHitHeight = 100.0f;

const int kSwordHitStart = 14;
const int kSwordHitEnd = 3;

// ボスの攻撃
const int kShotInterval = 180;

// カウンター
const float kCounterBulletSpeed = 23.0f;
const int kCounterShakeFrames = 12;
const float kCounterShakePower = 14.0f;

const int kCounterHitStopFrames = 3;
const int kCounterSlowFrames = 12;
const float kCounterSlowScale = 0.25f;

const int kCounterRetryCooldown = 30;
const int kJustCounterWindow = 8;

// ダッシュカウンター
const float kDashCounterBulletSpeed = 32.0f;
const int kDashCounterHitStopFrames = 5;
const int kDashCounterSlowFrames = 18;
const int kDashCounterShakeFrames = 16;
const float kDashCounterShakePower = 20.0f;
const int kDashCounterFlashFrames = 9;
const int kDashCounterImpactFrames = 24;

// プレイヤー
struct Player {
    float x, y;
    int direction;
    float velocityY;
    bool onGround;
};

// ダッシュ残像
struct DashAfterimage {
    float x, y;
    int timer;
};

// ダッシュ衝撃波
struct DashWave {
    float x, y;
    int timer;
};

// ボス
struct Boss {
    float x, y;
    int shotTimer;
};

// 弾の残像
const int kTrailLength = 12;

struct TrailPoint {
    float x, y;
};

// 弾
struct Bullet {
    float x, y;
    float vx, vy;
    bool active;
    bool reflected;
    bool dashReflected;

    TrailPoint trail[kTrailLength];
    int trailCount;
};

// 弾の最大数
const int kMaxBullets = 20;

// カウンターエフェクト
struct CounterEffect {
    float x, y;
    int timer;
    bool enhanced;
};

// 共通関数
void DrawBackground();

unsigned int WithAlpha(
    unsigned int rgb,
    int alpha
);

bool IsHit(
    float ax, float ay, float aw, float ah,
    float bx, float by, float bw, float bh
);
