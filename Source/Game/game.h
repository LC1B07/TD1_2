
#pragma once

#include <Novice.h>
#include <cmath>
#include <cstring>

// シーンの種類
enum Scene {
	SCENE_TITLE,         // タイトル
	SCENE_BGM,           // BGM
    SCENE_CREDITS,       // クレジット
    SCENE_TUTORIAL,      // チュートリアル
    SCENE_BOSS_SELECT,   // ボス選択
    SCENE_BOSS_A,        // ボスA
    SCENE_BOSS_B,        // ボスB
	SCENE_CLEAR,         // クリア
	SCENE_GAMEOVER       // ゲームオーバー
};

// 画面サイズ
const int kWidth = 1920;
const int kHeight = 1080;

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

// カウンター演出
struct CounterEffect {
    float x;
    float y;
    int timer;
    bool enhanced;
};

// ゲーム全体の状態
struct GameState {

    Player player;
    Boss boss;
    Bullet bullets[kMaxBullets];

    CounterEffect counterEffect;

    int counterCooldown = 0;
    int bossFlash = 0;

    int hitStopTimer = 0;
    int slowTimer = 0;

    float gameFrameAccumulator = 0.0f;

    int shakeTimer = 0;
    int shakeTotalFrames = kCounterShakeFrames;
    float shakeMaxPower = kCounterShakePower;

    int specialFlashTimer = 0;
    int specialImpactTimer = 0;

    float specialImpactX = 0.0f;
    float specialImpactY = 0.0f;

    int shakeX = 0;
    int shakeY = 0;

    // ダッシュ
    bool dashRequest = false;

    int dashTimer = 0;
    int dashCooldown = 0;

    float dashDirX = 0.0f;
    float dashDirY = 0.0f;

    DashWave dashWave;
    DashAfterimage dashImages[kDashAfterimageCount];

    int dashImageIndex = 0;

    // 剣
    int swordSwingTimer = 0;

    bool swordRequest = false;
    bool normalSwordRequest = false;
    bool normalSwordActive = false;

    bool swordHitBoss = false;

    int swordHitFlashTimer = 0;

    // カウンター
    int counterWindowTimer = 0;
    bool counterArmed = false;

    bool jumpRequest = false;

    bool justWindow = false;
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
