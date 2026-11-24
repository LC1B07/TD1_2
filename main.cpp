#include <Novice.h>
#include "Controller.h"
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <ctime>

// シーンの種類
enum Scene {
    SCENE_TITLE,
    SCENE_BOSS,
    SCENE_GAMEOVER,
    SCENE_CLEAR,
    SCENE_CREDITS // クレジット画面を追加
};

// 全シーン共通の背景色（パーティクルなし）
const int kWidth = 1920;
const int kHeight = 1080;
// ボスは画面右端に固定し、画面の高さ全体を覆う大型の矩形にする。
const float kBossWidth = 450.0f;
const float kBossHeight = 1080.0f;
const float kBossWireOffsetX = 12.0f;
const float kBossWireOffsetY = 360.0f;
void DrawBackground() {
    Novice::DrawBox(0, 0, kWidth, kHeight, 0.0f, 0x151826FF, kFillModeSolid);
}

// ガチャの背景パーティクルを移植。背景画像は塗りつぶさず光の粒だけ描画する。
const int kBackgroundParticleCount = 32;
void DrawBackgroundParticles(int frame) {
    Novice::SetBlendMode(kBlendModeNormal);
    for (int i = 0; i < kBackgroundParticleCount; ++i) {
        int x = (i * 191 + 83) % kWidth;
        int y = (i * 127 + 47 - frame / (2 + i % 3)) % kHeight;
        if (y < 0) y += kHeight;
        int radius = 2 + i % 4;
        // 外側の半透明の光と中心の明るい粒を重ねる。
        Novice::DrawBox(x - radius * 2, y - radius * 2, radius * 4, radius * 4,
            0.0f, 0x7BBFFF30u, kFillModeSolid);
        Novice::DrawBox(x, y, radius, radius, 0.0f, 0xD9F3FF90u, kFillModeSolid);
    }
}

// ガチャで使ったアルファブレンディング（色の下位8bitが透明度）
unsigned int WithAlpha(unsigned int rgb, int alpha) {
    if (alpha < 0) alpha = 0;
    if (alpha > 255) alpha = 255;
    return (rgb & 0xFFFFFF00u) | (unsigned int)alpha;
}

// タイトルの選択ボタンを描画する
void DrawMenuButton(int x, int y, const char* label, bool selected) {
    unsigned int fill = selected ? 0x293D59FF : 0x1D2536FF;
    unsigned int edge = selected ? 0x8BCBFFFF : 0x53627CFF;
    // 選択時は半透明の影とハイライトを重ねて立体感を出す
    Novice::DrawBox(x + 5, y + 7, 320, 64, 0.0f,
        WithAlpha(0x00000000u, 100), kFillModeSolid);
    Novice::DrawBox(x, y, 320, 64, 0.0f, fill, kFillModeSolid);
    if (selected) {
        Novice::DrawBox(x + 3, y + 3, 314, 9, 0.0f,
            WithAlpha(0x8BCBFF00u, 75), kFillModeSolid);
    }
    Novice::DrawBox(x, y, 320, 64, 0.0f, edge, kFillModeWireFrame);
    Novice::ScreenPrintf(x + 95, y + 23, "%s", label);
    if (selected) Novice::ScreenPrintf(x + 25, y + 23, ">");
}

// ボス戦の設定（60FPSを想定：180フレーム = 約3秒）
// A/Dで左右移動、SPACEでジャンプ。空中ダッシュ中はW/Sで上下方向も指定可能。Hは通常攻撃。
const float kGroundY = 930.0f;
const float kGravity = 0.55f;
const float kJumpPower = -12.0f;
const float kMoveSpeed = 5.0f;
// ダッシュ：左Shiftを押した瞬間に左右方向へ10ゲームフレーム高速移動
const float kDashSpeed = 16.0f;
const int kDashDuration = 10;
const int kDashCooldown = 30; // 実フレームで管理（スロー中の連打防止）
const int kDashWaveFrames = 18;
const int kDashAfterimageCount = 8;
// 剣の試作：Hで前方を大きく振る（通常攻撃だけ広い当たり判定）
const int kSwordSwingFrames = 16;
const float kSwordLength = 58.0f;
const float kSwordHitWidth = 100.0f;
const float kSwordHitHeight = 100.0f;
// 16Fの斬撃のうち中盤12Fのみ攻撃判定を出す
const int kSwordHitStart = 14;
const int kSwordHitEnd = 3;
// 攻撃は共通スケジューラから1種類ずつ選択する
const int kAttackWaitMin = 75;  // 攻撃終了後の最短待機（実フレーム）
const int kAttackWaitRandom = 76; // 0～75フレームを加算

const float kCounterBulletSpeed = 23.0f; // 反射弾の速さ（元は13）
const int kCounterShakeFrames = 12; // 強いシェイクの長さ
const float kCounterShakePower = 14.0f; // 揺れ幅（ピクセル）
// カウンター成功時：3F停止 → 12Fだけゲーム速度25% → 通常速度
const int kCounterHitStopFrames = 3;
const int kCounterSlowFrames = 12;
const float kCounterSlowScale = 0.25f;
const int kCounterRetryCooldown = 30; // 押した時点から30Fは再挑戦不可（連打対策）
const int kJustCounterWindow = 8; // Jを押してから8実フレームがジャスト判定
// ダッシュ中にジャストカウンターした場合の特別演出
const float kDashCounterBulletSpeed = 32.0f;
const int kDashCounterHitStopFrames = 5;
const int kDashCounterSlowFrames = 18;
const int kDashCounterShakeFrames = 16;
const float kDashCounterShakePower = 20.0f;
const int kDashCounterFlashFrames = 9;
const int kDashCounterImpactFrames = 24;

struct Player {
    float x, y;
    int direction; // 1:右向き、-1:左向き
    float velocityY;
    bool onGround;
};

// ダッシュ中に残すプレイヤーの残像
struct DashAfterimage {
    float x, y;
    int timer;
};

// 発動位置から広がる青い衝撃波
struct DashWave {
    float x, y;
    int timer;
};

// 龍のしっぽ拘束攻撃：地面の予告→地中から突き上げ→拘束→後隙
// 60FPS想定。ワイヤーが触れる瞬間にJでカウンターすると拘束を防げる。
enum WirePhase { WIRE_IDLE, WIRE_WARNING, WIRE_FLYING, WIRE_BOUND, WIRE_RECOVERY };
const int kWireInterval = 300;
const int kWireWarningFrames = 90; // 予告を1.5秒に延長（以前は0.5秒）
const int kWireMaxBoundFrames = 125;
const float kWireSpeed = 16.0f; // しっぽが地面から伸びる速度
struct WireAttack {
    WirePhase phase;
    int timer;
    float x, y;
    float vx, vy;
    float targetX, targetY;
    float groundX, groundY; // しっぽが地面から現れる固定位置
    int flashTimer;
    bool escaped;
};

// 巨大龍ブレス：右上の口からプレイヤーの位置へ照準を固定して発射。
// 予告線を見てジャンプ・空中ダッシュで射線から逃げる。
enum BreathPhase { BREATH_IDLE, BREATH_WARNING, BREATH_EXTEND, BREATH_HOLD, BREATH_RECOVERY };
const int kBreathInterval = 420; // 約7秒ごと（実フレーム）
const int kBreathWarningFrames = 75;
const int kBreathExtendFrames = 28;
const int kBreathHoldFrames = 65;
const int kBreathRecoveryFrames = 45;
const float kBreathRadius = 45.0f; // ブレスの半径
const float kDragonMouthOffsetX = 25.0f; // 右上にある龍の口（ボス左端基準）
const float kDragonMouthY = 115.0f;
struct DragonBreath {
    BreathPhase phase;
    int timer;
    int waitTimer;
    float startX, startY; // 龍の口
    float dirX, dirY;     // 予告開始時に固定した照準方向
    float length;         // 伸びたブレスの長さ
    int hitFlash;
    int hitCooldown;
};

// 画面外からの龍突進：予告→退避→左から高速突進→後隙。
// 突進はプレイヤーの高さを予告開始時に固定する。
enum ChargePhase { CHARGE_IDLE, CHARGE_WARNING, CHARGE_LEAVE, CHARGE_RUSH, CHARGE_FALL, CHARGE_DOWN, CHARGE_GETUP, CHARGE_RECOVERY };
const int kChargeWarningFrames = 85;  // 約1.4秒の予告
const int kChargeLeaveFrames = 24;    // 画面外へ移動する時間
const int kChargeRecoveryFrames = 45;
const float kChargeSpeed = 32.0f;
const float kChargeWidth = 230.0f;
const float kChargeHeight = 110.0f;
const int kChargeDownFrames = 240; // ダウン時間：60FPSで4秒
const int kChargeGetupFrames = 35;
const float kChargeFallGravity = 1.25f;
struct DragonCharge {
    ChargePhase phase;
    int timer;
    float x, y;
    float targetY;
    float vx, vy; // ランダム角度の突進速度
    float angle;  // 描画用角度（ラジアン）
    int hitFlash;
    bool hitOnce;
    float fallVelocityY; // パリィ後の落下速度
    float fallSpin;      // 墜落中の回転角度
    int dustTimer;       // 着地の土煙（実フレーム）
    int downHitFlash;    // ダウン中に剣が命中した演出
};

const int kBossMaxHP = 1000;

struct Boss {
    float x, y;
    int shotTimer;
    int hp; // 龍の現在HP
};

// 反射弾の残像：過去の位置を最大12個記録する
const int kTrailLength = 12;
struct TrailPoint {
    float x, y;
};

struct Bullet {
    float x, y;
    float vx, vy;
    bool active;
    bool reflected; // カウンターで反射した弾かどうか
    bool dashReflected; // ダッシュジャストカウンターで強化された弾か
    bool giant; // 巨大魔力球か
    int reflectLevel; // 0:未反射 1:通常パリィ 2:ジャスト 3:ダッシュジャスト
    int homingTimer; // 巨大魔力球の追尾残り時間。反射すると追尾しない
    int appearanceTimer; // 魔力球が回転しながら大きくなる登場演出の経過フレーム
    TrailPoint trail[kTrailLength]; // 古い位置を保持
    int trailCount;                 // 現在記録している数
};

const int kMaxBullets = 20;

// カウンター成功時の光エフェクト（画像・パーティクル配列不要）
struct CounterEffect {
    float x, y;
    int timer;
    bool enhanced; // ダッシュジャストカウンターの大きな衝撃波か
};

// カウンター成功時の画像演出。画像は Resources フォルダから読み込む。
// 表示は実フレームで進むので、ヒットストップ・スロー中も自然に動く。
enum CounterTextType { COUNTER_TEXT_NONE, COUNTER_TEXT_PARRY, COUNTER_TEXT_PERFECT, COUNTER_TEXT_BREAK };
const int kCounterTextFrames = 32;
struct CounterTextEffect {
    CounterTextType type;
    int timer;
    float x, y; // 画面座標（シェイクは加算しない）
};

void StartCounterText(CounterTextEffect& effect, CounterTextType type, float x, float y) {
    effect = { type, kCounterTextFrames, x, y };
}

void DrawCounterText(const CounterTextEffect& effect, int parryTexture,
    int perfectTexture, int breakTexture) {
    if (effect.timer <= 0 || effect.type == COUNTER_TEXT_NONE) return;
    int texture = -1;
    if (effect.type == COUNTER_TEXT_PARRY) texture = parryTexture;
    if (effect.type == COUNTER_TEXT_PERFECT) texture = perfectTexture;
    if (effect.type == COUNTER_TEXT_BREAK) texture = breakTexture;
    if (texture < 0) return;

    const int elapsed = kCounterTextFrames - effect.timer;
    // 128x128の画像内に文字が描かれている。透明部分も含めて中央揃え。
    // 0～5Fで勢いよく拡大、6～13Fで落ち着き、後半は上へ浮かびながら消える。
    float scale = 1.0f;
    if (elapsed < 6) scale = 0.85f + 0.25f * (float)elapsed;
    else if (elapsed < 14) scale = 2.10f - 0.065f * (float)(elapsed - 6);
    else scale = 1.58f;
    int alpha = 255;
    if (effect.timer < 12) alpha = effect.timer * 255 / 12;
    float rise = elapsed > 12 ? (float)(elapsed - 12) * 2.0f : 0.0f;
    // DrawSpriteの位置は画像左上なので、画像サイズを基準に中央へ配置する。
    Novice::DrawSprite((int)(effect.x - 64.0f * scale),
        (int)(effect.y - rise - 64.0f * scale), texture,
        scale, scale, 0.0f, WithAlpha(0xFFFFFFFFu, alpha));
}

void DrawCounterEffect(const CounterEffect& e, int shakeX, int shakeY) {
    if (e.timer <= 0) return;
    const int elapsed = 30 - e.timer;
    const float t = (float)e.timer / 30.0f;
    const int cx = (int)e.x + shakeX;
    const int cy = (int)e.y + shakeY;
    // 2重の衝撃波：時間が進むほど大きく、薄くなる
    for (int i = 0; i < 2; ++i) {
        int radius = 12 + elapsed * 5 - i * 18;
        if (radius <= 0) continue;
        unsigned int color = WithAlpha(0x72DFFFFFu, (int)(190.0f * t));
        Novice::DrawEllipse(cx, cy, radius, radius, 0.0f, color, kFillModeWireFrame);
    }
    // ダッシュジャストカウンター限定：三重の巨大衝撃波と追加の光線
    if (e.enhanced) {
        for (int i = 0; i < 3; ++i) {
            int radius = 30 + elapsed * 12 - i * 28;
            if (radius > 0) {
                Novice::DrawEllipse(cx, cy, radius, radius, 0.0f,
                    WithAlpha(0xD7FFFFFFu, (int)(225.0f * t) - i * 25), kFillModeWireFrame);
            }
        }
        for (int i = 0; i < 12; ++i) {
            float a = 6.2831853f * i / 12.0f;
            float dx = cosf(a), dy = sinf(a);
            float inner = 24.0f + elapsed * 7.0f;
            float outer = inner + 80.0f;
            Novice::DrawLine((int)(cx + dx * inner), (int)(cy + dy * inner),
                (int)(cx + dx * outer), (int)(cy + dy * outer),
                WithAlpha(0xA4F2FFFFu, (int)(200.0f * t)));
        }
    }
    // 十字閃光：最初の数フレームだけ強く光る
    if (elapsed < 7) {
        int len = 65 - elapsed * 6;
        unsigned int flash = WithAlpha(0xFFFFFFFFu, 255 - elapsed * 30);
        Novice::DrawLine(cx - len, cy, cx + len, cy, flash);
        Novice::DrawLine(cx, cy - len, cx, cy + len, flash);
        Novice::DrawEllipse(cx, cy, 15, 15, 0.0f,
            WithAlpha(0xA8FFFFFFu, 170 - elapsed * 20), kFillModeSolid);
    }
    // 放射状の青いスパーク（ランダム値を使わず安定描画）
    const float pi = 3.14159265f;
    for (int i = 0; i < 16; ++i) {
        float angle = (2.0f * pi * i / 16.0f) + (i % 3) * 0.15f;
        float dist = 10.0f + elapsed * (2.0f + (i % 4) * 0.6f);
        float length = 8.0f + (i % 4) * 5.0f;
        float dx = cosf(angle), dy = sinf(angle);
        Novice::DrawLine((int)(e.x + dx * dist) + shakeX, (int)(e.y + dy * dist) + shakeY,
            (int)(e.x + dx * (dist + length)) + shakeX, (int)(e.y + dy * (dist + length)) + shakeY,
            WithAlpha(0x70CFFFFFu, (int)(235.0f * t)));
    }
}


bool IsHit(float ax, float ay, float aw, float ah,
    float bx, float by, float bw, float bh) {
    return ax < bx + bw && ax + aw > bx &&
        ay < by + bh && ay + ah > by;
}

void ResetBossBattle(Player& player, Boss& boss, Bullet bullets[]) {
    player = { 210.0f, kGroundY - 48.0f, 1, 0.0f, true };
    boss = { kWidth - kBossWidth, 0.0f, 0, kBossMaxHP };
    for (int i = 0; i < kMaxBullets; ++i) bullets[i].active = false;
}

// 剣の描画。振り始めは上、途中は前、振り終わりは下を向く。
// 攻撃判定は別途、更新処理で計算する。
void DrawSword(const Player& player, int swingTimer, int shakeX, int shakeY) {
    const float pi = 3.14159265f;
    const float cx = player.x + 18.0f + player.direction * 12.0f + shakeX;
    const float cy = player.y + 24.0f + shakeY;
    const bool swinging = swingTimer > 0;
    const float progress = swinging ?
        (float)(kSwordSwingFrames - swingTimer) / (float)kSwordSwingFrames : 0.0f;

    // 待機時は斜め前上に構える。攻撃時は上から下へ約180度振る。
    const float baseAngle = swinging ? (-pi * 0.75f + progress * pi) : (-pi * 0.22f);
    const float angle = player.direction == 1 ? baseAngle : pi - baseAngle;

    // 振っている間だけ過去の角度に薄い残光を重ねる。
    if (swinging) {
        for (int i = 1; i <= 4; ++i) {
            float oldProgress = progress - 0.055f * i;
            if (oldProgress < 0.0f) continue;
            float oldBase = -pi * 0.75f + oldProgress * pi;
            float oldAngle = player.direction == 1 ? oldBase : pi - oldBase;
            float ex = cx + cosf(oldAngle) * kSwordLength;
            float ey = cy + sinf(oldAngle) * kSwordLength;
            Novice::DrawLine((int)cx, (int)cy, (int)ex, (int)ey,
                WithAlpha(0x63CFFFFFu, 100 - i * 19));
        }
    }

    const float dx = cosf(angle), dy = sinf(angle);
    const float tipX = cx + dx * kSwordLength;
    const float tipY = cy + dy * kSwordLength;
    // 刃の外側と明るい芯を2本の線で表現する。
    Novice::DrawLine((int)cx, (int)cy, (int)tipX, (int)tipY, 0x52BFFFFFu);
    Novice::DrawLine((int)(cx - dy * 2.0f), (int)(cy + dx * 2.0f),
        (int)(tipX - dy * 2.0f), (int)(tipY + dx * 2.0f), 0xE5F9FFFFu);
    // 柄と鍔
    Novice::DrawLine((int)(cx - dx * 12.0f), (int)(cy - dy * 12.0f),
        (int)cx, (int)cy, 0x326E9BFFu);
    Novice::DrawLine((int)(cx - dy * 7.0f), (int)(cy + dx * 7.0f),
        (int)(cx + dy * 7.0f), (int)(cy - dx * 7.0f), 0xFFFFFFFFu);
}

// 描画専用の疑似乱数：毎フレームの炎を安定してちらつかせる。
// rand()を使わないのでボスのランダム攻撃順には影響しない。
unsigned int FireHash(unsigned int x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    return x ^ (x >> 16);
}

// 炎の描画専用。ゲーム内の攻撃範囲・衝突判定は元のまま。
// maru2.png (64x64) を中心位置と半径を指定して描画する。
// 元の円と同じ範囲に収まるように、直径を基準に拡縮する。
void DrawBreathCircleSprite(int texture, float cx, float cy, float radius, unsigned int color) {
    if (texture < 0 || radius <= 0.0f) return;
    const float diameter = radius * 2.0f;
    const float scale = diameter / 64.0f;
    Novice::DrawSprite((int)(cx - radius), (int)(cy - radius),
        texture, scale, scale, 0.0f, color);
}

void DrawDragonFireBreath(const DragonBreath& breath, int breathCircleTexture, int shakeX, int shakeY) {
    if (breath.phase != BREATH_WARNING && breath.phase != BREATH_EXTEND &&
        breath.phase != BREATH_HOLD) return;

    const float sx = breath.startX;
    const float sy = breath.startY;
    const float dx = breath.dirX;
    const float dy = breath.dirY;
    const float nx = -dy;
    const float ny = dx;
    const float pi = 3.14159265f;
    const float time = (float)breath.timer;

    if (breath.phase == BREATH_WARNING) {
        // 既存の攻撃範囲の予告線を残す。
        const float ex = sx + dx * 1600.0f;
        const float ey = sy + dy * 1600.0f;
        const int alpha = 65 + (breath.timer % 16) * 8;
        for (int side = -1; side <= 1; ++side) {
            float offset = side * kBreathRadius;
            Novice::DrawLine((int)(sx + nx * offset) + shakeX,
                (int)(sy + ny * offset) + shakeY,
                (int)(ex + nx * offset) + shakeX,
                (int)(ey + ny * offset) + shakeY,
                WithAlpha(0x36BFFFFFu, alpha));
        }
        // 口元に炎の粒が吸い込まれる。予告後半ほど密集し、明るくなる。
        const float charge = (float)breath.timer / (float)kBreathWarningFrames;
        for (int i = 0; i < 20; ++i) {
            unsigned int seed = FireHash((unsigned int)i * 971u + 19u);
            float angle = (float)(seed % 628u) * 0.01f;
            float phase = fmodf(charge * (1.7f + (i % 4) * 0.13f) + (float)(i % 17) / 17.0f, 1.0f);
            float radius = (1.0f - phase) * (90.0f + (float)(seed % 65u));
            float px = sx + cosf(angle) * radius;
            float py = sy + sinf(angle) * radius;
            int size = 2 + (int)(phase * 5.0f);
            unsigned int color = (i % 3 == 0) ? 0xE6FFFFFFu :
                ((i % 3 == 1) ? 0x62DFFFFFu : 0x168DFFFFu);
            Novice::DrawEllipse((int)px + shakeX, (int)py + shakeY,
                size, size, 0.0f, color, kFillModeSolid);
        }
        int mouthRadius = 9 + (int)(charge * 20.0f) + (breath.timer % 5);
        // 予告中の口元の円も maru2.png で表示する。
        DrawBreathCircleSprite(breathCircleTexture, sx + shakeX, sy + shakeY,
            (float)(mouthRadius + 12), WithAlpha(0x126BD000u, 75 + (int)(charge * 65)));
        DrawBreathCircleSprite(breathCircleTexture, sx + shakeX, sy + shakeY,
            (float)mouthRadius, 0x38BFFFFFu);
        DrawBreathCircleSprite(breathCircleTexture, sx + shakeX, sy + shakeY,
            (float)(mouthRadius / 2), 0xE8FFFFFFu);
        Novice::ScreenPrintf(430, 105, "DRAGON BREATH WARNING!");
        return;
    }

    // 画面外へ延びる炎は描画せず、衝突判定には一切影響させない。
    float length = breath.length;
    if (length <= 0.0f) return;
    if (length > 1600.0f) length = 1600.0f;
    // 炎は遠くほど広がる。中心の当たり判定は従来の半径45pxのまま。
    // 外側の炎・火の粉は演出用で、当たり判定を持たない。
    // 2000スプライト上限対策：炎の断面を24px間隔に間引く。
    // 1断面4枚 + 火の粉64枚 + 着弾演出最大17枚に制限。
    const float kFireSegmentStep = 24.0f;
    const int segmentCount = (int)(length / kFireSegmentStep) + 1;
    for (int i = 0; i < segmentCount; ++i) {
        float d = (float)i * kFireSegmentStep;
        if (d > length) d = length;
        float spread = 14.0f + 0.068f * d;
        if (spread > 112.0f) spread = 112.0f;
        float sway = sinf(d * 0.037f - time * 0.23f) * (4.0f + d * 0.012f)
            + sinf(d * 0.083f + time * 0.17f) * 6.0f;
        float cx = sx + dx * d + nx * sway;
        float cy = sy + dy * d + ny * sway;
        float jag = sinf(d * 0.13f + time * 0.35f) * 8.0f;
        int outer = (int)(spread + jag);
        if (outer < 9) outer = 9;
        // 以前の4層の円を maru2.png に置き換える。
        // 半透明の青い本体に、従来の maru.png 加算発光を重ねる。
        DrawBreathCircleSprite(breathCircleTexture, cx + shakeX, cy + shakeY,
            (float)outer, 0x096BED48u);
        DrawBreathCircleSprite(breathCircleTexture, cx + shakeX, cy + shakeY,
            outer * 0.69f, 0x159FFF68u);
        DrawBreathCircleSprite(breathCircleTexture, cx + shakeX, cy + shakeY,
            outer * 0.38f, 0x62DFFF88u);
        DrawBreathCircleSprite(breathCircleTexture, cx + shakeX, cy + shakeY,
            outer * 0.18f + 2.0f, 0xE9FFFFFFu);

    }

    // 流れる火の粉：発射方向に進みながら左右へ散
    for (int i = 0; i < 64; ++i) {
        unsigned int seed = FireHash((unsigned int)i * 1381u + 29u);
        float u = fmodf((float)(seed % 1000u) / 1000.0f + time * (0.009f + (i % 4) * 0.002f), 1.0f);
        float d = u * length;
        float spread = 19.0f + d * 0.11f;
        float side = ((float)((seed >> 10) % 2001u) / 1000.0f - 1.0f) * spread;
        float flicker = sinf(time * 0.31f + i * 2.1f) * 5.0f;
        float px = sx + dx * d + nx * (side + flicker);
        float py = sy + dy * d + ny * (side + flicker);
        int r = 2 + (int)((seed >> 22) % 5u);
        unsigned int color = (i % 3 == 0) ? 0xE5FFFFFFu :
            ((i % 3 == 1) ? 0x66DFFFFFu : 0x178EFFFFu);
        Novice::DrawEllipse((int)px + shakeX, (int)py + shakeY,
            r, r, 0.0f, color, kFillModeSolid);
    }

    // 地面(kGroundY)と炎の中心線が交差する位置に着弾炎を描画。
    if (dy > 0.001f) {
        float impactDistance = (kGroundY - sy) / dy;
        if (impactDistance >= 0.0f && impactDistance <= length) {
            float ix = sx + dx * impactDistance;
            int iy = (int)kGroundY;
            for (int i = 0; i < 12; ++i) {
                float angle = pi + pi * (float)i / 11.0f;
                float burst = 16.0f + (float)((i * 37 + breath.timer * 11) % 95);
                float px = ix + cosf(angle) * burst;
                float py = (float)iy + sinf(angle) * burst * 0.7f;
                int r = 3 + (i % 5);
                unsigned int color = i % 2 ? 0x35C8FFE0u : 0xA3F0FFF0u;
                Novice::DrawEllipse((int)px + shakeX, (int)py + shakeY,
                    r, r, 0.0f, color, kFillModeSolid);
            }
            for (int i = 0; i < 3; ++i) {
                int rx = 18 + i * 17 + (breath.timer % 8);
                Novice::DrawEllipse((int)ix + shakeX, iy + shakeY,
                    rx, 8 + i * 3, 0.0f,
                    WithAlpha(0x198AE000u, 130 - i * 20), kFillModeWireFrame);
            }
        }
    }
    Novice::ScreenPrintf(510, 105, "DRAGON BREATH!");
}

// maru.png (64x64) を加算合成で重ねるブレス専用の発光演出。
// 当たり判定や攻撃タイマーは変更せず、描画だけを追加する。
void DrawBreathGlow(const DragonBreath& breath, int maruTexture, int shakeX, int shakeY) {
    if (maruTexture < 0) return; // 画像が読み込めなかった場合は描画しない
    if (breath.phase != BREATH_WARNING && breath.phase != BREATH_EXTEND &&
        breath.phase != BREATH_HOLD) return;

    // DrawSprite は左上座標なので、64x64の画像を中心基準で描画する。
    const float imageSize = 64.0f;
    const float sx = breath.startX + (float)shakeX;
    const float sy = breath.startY + (float)shakeY;
    const float flicker = 1.0f + 0.08f * sinf((float)breath.timer * 0.45f);

    Novice::SetBlendMode(kBlendModeAdd);

    if (breath.phase == BREATH_WARNING) {
        // 発射前は龍の口元に光を溜める。予告時間に応じて少しずつ強くする。
        float charge = (float)breath.timer / (float)kBreathWarningFrames;
        if (charge > 1.0f) charge = 1.0f;
        const float outerSize = (85.0f + 105.0f * charge) * flicker;
        const float innerSize = (30.0f + 42.0f * charge) * flicker;
        Novice::DrawSprite((int)(sx - outerSize * 0.5f), (int)(sy - outerSize * 0.5f),
            maruTexture, outerSize / imageSize, outerSize / imageSize, 0.0f,
            WithAlpha(0x0D78FF00u, 42 + (int)(charge * 55.0f)));
        Novice::DrawSprite((int)(sx - innerSize * 0.5f), (int)(sy - innerSize * 0.5f),
            maruTexture, innerSize / imageSize, innerSize / imageSize, 0.0f,
            WithAlpha(0x37C5FF00u, 100 + (int)(charge * 95.0f)));
        Novice::DrawSprite((int)(sx - innerSize * 0.22f), (int)(sy - innerSize * 0.22f),
            maruTexture, innerSize * 0.44f / imageSize, innerSize * 0.44f / imageSize, 0.0f,
            WithAlpha(0xE0F9FF00u, 145 + (int)(charge * 100.0f)));
    }
    else {
        // ブレスに沿って光を重ねる。描画枚数を抑えるため36px間隔にする。
        float length = breath.length;
        if (length > 1600.0f) length = 1600.0f;
        for (float d = 0.0f; d <= length; d += 36.0f) {
            float cx = sx + breath.dirX * d;
            float cy = sy + breath.dirY * d;
            float spread = 14.0f + 0.07f * d;
            if (spread > 112.0f) spread = 112.0f;
            float outerSize = (spread * 2.5f + 38.0f) * flicker;
            float coreSize = (spread * 0.7f + 18.0f) * flicker;
            // 広く薄い青い光
            Novice::DrawSprite((int)(cx - outerSize * 0.5f), (int)(cy - outerSize * 0.5f),
                maruTexture, outerSize / imageSize, outerSize / imageSize, 0.0f,
                WithAlpha(0x0D78FF00u, 46));
            // 中心付近の明るい水色の発光
            Novice::DrawSprite((int)(cx - coreSize * 0.5f), (int)(cy - coreSize * 0.5f),
                maruTexture, coreSize / imageSize, coreSize / imageSize, 0.0f,
                WithAlpha(0x72DFFF00u, 120));
            // 小さな青白い芯を重ね、中心だけを強く光らせる。
            const float brightSize = coreSize * 0.48f;
            Novice::DrawSprite((int)(cx - brightSize * 0.5f), (int)(cy - brightSize * 0.5f),
                maruTexture, brightSize / imageSize, brightSize / imageSize, 0.0f,
                WithAlpha(0xE7FAFF00u, 105));
        }
        // 龍の口元の強い発光
        const float mouthSize = 110.0f * flicker;
        Novice::DrawSprite((int)(sx - mouthSize * 0.5f), (int)(sy - mouthSize * 0.5f),
            maruTexture, mouthSize / imageSize, mouthSize / imageSize, 0.0f,
            WithAlpha(0xBDF4FF00u, 195));
    }

    // 後続の画像やUIが加算合成にならないよう必ず元に戻す。
    Novice::SetBlendMode(kBlendModeNormal);
}

// 巨大魔力球用の発光。ブレスと同じ maru.png を加算合成で重ねる。
// 魔法陣の模様を隠さないよう、外側の光は薄く、中心は小さく明るくする。
// 呼び出し元で加算合成に切り替えてから描画する。
void DrawGiantOrbGlow(float cx, float cy, float radius, int maruTexture,
    float time, bool enhanced) {
    if (maruTexture < 0) return;
    // 現在のブレスと同じ64x64の画像を想定。サイズ変更時はここを調整。
    const float imageSize = 64.0f;
    const float pulse = 1.0f + 0.07f * sinf(time * 0.22f);
    const float strength = enhanced ? 1.25f : 1.0f;
    const float outerSize = radius * 3.6f * pulse * strength;
    const float middleSize = radius * 2.6f * pulse * strength;
    const float coreSize = radius * 0.95f * pulse;
    // 外側の広い青い光
    Novice::DrawSprite((int)(cx - outerSize * 0.5f), (int)(cy - outerSize * 0.5f),
        maruTexture, outerSize / imageSize, outerSize / imageSize, 0.0f,
        WithAlpha(0x1479FF00u, enhanced ? 85 : 55));
    // 魔法陣を包む水色の光
    Novice::DrawSprite((int)(cx - middleSize * 0.5f), (int)(cy - middleSize * 0.5f),
        maruTexture, middleSize / imageSize, middleSize / imageSize, 0.0f,
        WithAlpha(0x42C6FF00u, enhanced ? 115 : 80));
    // 中心の小さな青白い発光
    Novice::DrawSprite((int)(cx - coreSize * 0.5f), (int)(cy - coreSize * 0.5f),
        maruTexture, coreSize / imageSize, coreSize / imageSize, 0.0f,
        WithAlpha(0xD5F6FF00u, enhanced ? 140 : 105));
}

// 雷の画像2枚を重ねて描画する。画像はどちらも128x1080。
// kaminari1: 半透明の雷本体 / kaminari2: 加算合成する明るい芯。
// x は雷の中心位置、bottomY は雷の下端。
void DrawImageLightning(int texture1, int texture2, float x, float bottomY, int timer) {
    if (timer <= 0) return;
    const float imageWidth = 128.0f;
    const float imageHeight = 1080.0f;
    const float scaleX = 1.4f;
    const float scaleY = 1.0f;
    const float width = imageWidth * scaleX;
    const float height = imageHeight * scaleY;
    const int left = (int)(x - width * 0.5f);
    const int top = (int)(bottomY - height);
    // 最初に強く光り、最後の数フレームで消える。
    const int alpha = timer < 7 ? timer * 255 / 7 : 255;
    const float flicker = 1.0f + 0.045f * sinf((float)timer * 3.7f);
    const int glowLeft = (int)(x - width * flicker * 0.5f);
    const int glowTop = (int)(bottomY - height);

    // 本体は通常合成。白い背景を残さず、元画像の透明度を活かす。
    Novice::SetBlendMode(kBlendModeNormal);
    if (texture1 >= 0) {
        Novice::DrawSprite(left, top, texture1, scaleX, scaleY, 0.0f,
            WithAlpha(0xB5DFFFFFu, alpha));
    }
    // 白い芯を加算合成で重ねる。雷の中心座標を共通にしてずれを防ぐ。
    Novice::SetBlendMode(kBlendModeAdd);
    if (texture2 >= 0) {
        Novice::DrawSprite(glowLeft, glowTop, texture2,
            scaleX * flicker, scaleY, 0.0f, WithAlpha(0xB6E9FFFFu, alpha));
        // 少し広い発光層。画像を増やさず同じ素材を薄く重ねる。
        Novice::DrawSprite((int)(x - width * 0.77f), top, texture2,
            scaleX * 1.54f, scaleY, 0.0f,
            WithAlpha(0x277CFF00u, alpha * 55 / 255));
    }
    Novice::SetBlendMode(kBlendModeNormal);
}

// 突進画像（500x120）を共通の中心と角度で描画する。
void DrawChargeImage(int texture, float cx, float cy, float angle, unsigned int color) {
    if (texture < 0) return;
    const float hw = kChargeWidth * 0.5f;
    const float hh = kChargeHeight * 0.5f;
    const float lx[4] = { -hw, hw, -hw, hw };
    const float ly[4] = { -hh, -hh, hh, hh };
    int x[4], y[4];
    for (int i = 0; i < 4; ++i) {
        x[i] = (int)roundf(cx + lx[i] * cosf(angle) - ly[i] * sinf(angle));
        y[i] = (int)roundf(cy + lx[i] * sinf(angle) + ly[i] * cosf(angle));
    }
    Novice::DrawQuad(x[0], y[0], x[1], y[1], x[2], y[2], x[3], y[3],
        0, 0, 500, 120, texture, color);
}

const char kWindowTitle[] = "LC1D_03_イシダ_ノリヒコ_タイトル";

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

    // ライブラリの初期化
    Novice::Initialize(kWindowTitle, kWidth, kHeight);
    // 動く背景の4枚の画像（ボス戦専用）
    const int bgTexture = Novice::LoadTexture("./Resources/haikei.png");
    // 地面画像（1920x1080、地面の上端は896px）
    const int groundTexture = Novice::LoadTexture("./Resources/jimen.png");
    const int moonTexture = Novice::LoadTexture("./Resources/tuki.png");
    
    float cloudScroll = 0.0f;
    float mistScroll = 0.0f;
    float backgroundTime = 0.0f;

    Controller::Initialize(); // ゲームパッドのデッドゾーンを初期化
    srand((unsigned int)time(nullptr)); // 攻撃順序を毎回ランダムにする
    // カウンター表示画像（実行ファイルの作業フォルダからの相対パス）
    const int parryTexture = Novice::LoadTexture("./Resources/PARRY.png");
    const int perfectTexture = Novice::LoadTexture("./Resources/PERFECT.png");
    const int breakTexture = Novice::LoadTexture("./Resources/BREAK.png");
    // ボス待機アニメーション：7680x1080の横4コマ画像を1枚のまま使用。
    // 各フレームは1920x1080。Resources/ryu.png に配置する。
    const int kBossIdleFrameCount = 4;
    const int kBossIdleFrameDuration = 10;
    const int kBossIdleFrameWidth = 450;
    const int kBossIdleFrameHeight = 1080;
    const int bossIdleTexture = Novice::LoadTexture("./Resources/ryu.png");
    int bossIdleTimer = 0;
    int giantGlowFrame = 0; // 魔力球の光を滑らかに脈動させる描画用タイマー
    int bossIdleFrame = 0; // 0～3の4コマを順番に再生
    //ブレンディングで利用するまるpng
    int maruTexture = Novice::LoadTexture("./Resources/maru.png");
    // 巨大魔力球の魔法陣（256x256）
    const int giantOrbTexture = Novice::LoadTexture("./Resources/kougeki1.png");
    // ブレス本体の円形描画を置き換える64x64画像
    int breathCircleTexture = Novice::LoadTexture("./Resources/maru2.png");

    // 突進本体と加算発光の画像（500x120）
    const int chargeTexture1 = Novice::LoadTexture("./Resources/tosin1.png");
    const int chargeTexture2 = Novice::LoadTexture("./Resources/tosin2.png");

    // 雷の描画用画像（Resourcesフォルダに配置）
    const int lightningTexture1 = Novice::LoadTexture("./Resources/kaminari1.png");
    const int lightningTexture2 = Novice::LoadTexture("./Resources/kaminari2.png");
    // 突進と連動する3本の落雷。位置は突進予告時に決定する。
    const int kRushLightningCount = 3;
    const int kRushLightningDuration = 18;
    const int kRushLightningInterval = 12;
    float lightningX[kRushLightningCount] = {};
    int lightningTimer[kRushLightningCount] = {};
    bool lightningHit[kRushLightningCount] = {};
    bool rushLightningReady = false;

    // キー入力結果を受け取る箱
    char keys[256] = { 0 };
    char preKeys[256] = { 0 };

    Scene currentScene = SCENE_TITLE; // 最初はタイトル画面
    int titleSelection = 0; // 0: GAME START、1: CREDITS
    Player player = {};
    Boss boss = {};
    Bullet bullets[kMaxBullets] = {};
    ResetBossBattle(player, boss, bullets);
    CounterEffect counterEffect = {};
    CounterTextEffect counterText = {};
    int counterCooldown = 0;
    int bossFlash = 0;
    int hitStopTimer = 0; // 残りヒットストップ時間（実フレーム）
    int slowTimer = 0;    // 残りスロー時間（実フレーム）
    float gameFrameAccumulator = 0.0f; // 0.25倍速でも更新を正しく間引くための蓄積値
    int shakeTimer = 0; // カウンター成功時の画面シェイク
    int shakeTotalFrames = kCounterShakeFrames;
    float shakeMaxPower = kCounterShakePower;
    int specialFlashTimer = 0; // ダッシュカウンター成功時の画面フラッシュ
    int specialImpactTimer = 0; // 強化弾がボスに命中したときの爆発
    float specialImpactX = 0.0f, specialImpactY = 0.0f;
    int shakeX = 0;
    int shakeY = 0;
    // ダッシュの状態と演出
    bool dashRequest = false;
    int dashTimer = 0;
    int dashDodgeEffect = 0; // ダッシュ回避成功の表示時間
    int dashCooldown = 0;
    float dashDirX = 0.0f;
    float dashDirY = 0.0f;
    DashWave dashWave = {};
    DashAfterimage dashImages[kDashAfterimageCount] = {};
    int dashImageIndex = 0;
    int swordSwingTimer = 0; // 残り斬撃フレーム（ゲーム速度に合わせて進める）
    bool swordRequest = false; // スロー中の入力を取りこぼさない
    bool normalSwordRequest = false; // Hの通常攻撃のみを区別
    bool normalSwordActive = false; // Jの剣振りでは通常攻撃判定を出さない
    bool swordHitBoss = false; // 1回の斬撃でボスへの命中は1回だけ
    int swordHitFlashTimer = 0; // 通常攻撃の命中確認用
    int counterWindowTimer = 0; // Jを押した後のジャスト判定残り時間
    bool counterArmed = false; // この押下でジャストカウンター可能か
    bool jumpRequest = false; // ヒットストップ中でもジャンプ入力を保持
    DragonBreath breath = {};
    DragonBreath breathSecond = {}; // 2本目のランダム方向ブレス
    DragonCharge charge = {};
    WireAttack wire = {};
    int wireWait = 0; // 互換用（ワイヤーの自動発動には使わない）
    int nextAttackWait = kAttackWaitMin;
    int lastAttack = -1; // 0:通常弾 1:しっぽ 2:ブレス 3:突進
    bool attackInProgress = false;
    int wireEscapeEffect = 0;
    // 巨大魔力球のチャージ。0の攻撃枠でのみ発動する。
    int giantChargeTimer = 0;
    const int kGiantChargeFrames = 75;
    const float kGiantRadius = 62.0f;
    const int kGiantOrbCount = 5; // 同時に発射する魔力球の数
    const int kGiantHomingFrames = 30; // 発射後約0.5秒だけ追尾（60FPS想定）
    const float kGiantOrbSpeed = 8.0f;
    const int kGiantAppearFrames = 24; // 24Fかけて回転しながら拡大（描画のみ）
    const float kGiantHomingTurn = 0.035f; // 1フレームの方向補正率（小さいほど弱い追尾）

    // ウィンドウの×ボタンが押されるまでループ
    while (Novice::ProcessMessage() == 0) {
        // フレームの開始
        Novice::BeginFrame();
        ++giantGlowFrame;
        // 登場アニメーションは実フレームで更新。ヒットストップ中も止めない。
        for (int i = 0; i < kMaxBullets; ++i) {
            if (bullets[i].active && bullets[i].giant &&
                bullets[i].appearanceTimer < kGiantAppearFrames) {
                ++bullets[i].appearanceTimer;
            }
        }

        // キー入力を受け取る
        memcpy(preKeys, keys, 256);
        Novice::GetHitKeyStateAll(keys);

        // ゲームパッド入力を既存のキー操作に合成する。
        // キーボード操作を残したまま、既存の当たり判定・パリィ受付・シーン遷移を共用。
        Controller::Update();
        if (Controller::IsConnected()) {
            // Noviceのボタン番号はkPadButton.txtの対応表を使用する。0.3はコントローラーのでっとゾーン
            ////////////////////////////////////////////////////////////////////////////////////////////////////////
            ////////////////////////////////////////////////////////////////////////////////////////////////////////
            const bool left = Controller::IsDown(kPadButton2) || Controller::GetLeftStickX() < -0.10f;
            const bool right = Controller::IsDown(kPadButton3) || Controller::GetLeftStickX() > 0.10f;
            const bool upPad = Controller::IsDown(kPadButton0) || Controller::GetLeftStickY() < 0.10f;
            const bool downPad = Controller::IsDown(kPadButton1) || Controller::GetLeftStickY() > -0.10f;
            /////////////////////////////////////////////////////////////////////////////////////////////////////////
            /////////////////////////////////////////////////////////////////////////////////////////////////////////
            if (left) keys[DIK_A] = 1;
            if (right) keys[DIK_D] = 1;
            if (upPad) keys[DIK_W] = 1;
            if (downPad) keys[DIK_S] = 1;

            // Aボタンはシーンによって役割を変更する。
            switch (currentScene) {
            case SCENE_BOSS:
                if (Controller::IsDown(kPadButton10)) keys[DIK_SPACE] = 1; // A: ジャンプ
                break;
            case SCENE_TITLE:
            case SCENE_GAMEOVER:
            case SCENE_CLEAR:
            case SCENE_CREDITS:
                if (Controller::IsDown(kPadButton10)) keys[DIK_RETURN] = 1; // A: 決定
                break;
            default:
                break;
            }

            if (Controller::IsDown(kPadButton11)) keys[DIK_LSHIFT] = 1; // B: ダッシュ
            if (Controller::IsDown(kPadButton15)) keys[DIK_H] = 1; // R2: 剣
            if (Controller::IsDown(kPadButton13)) keys[DIK_J] = 1; // Y: パリィ
            if (Controller::IsDown(kPadButton9)) keys[DIK_J] = 1; // R1: パリィ
            if (Controller::IsDown(kPadButton14)) keys[DIK_LSHIFT] = 1; // L2: ダッシュ
            if (Controller::IsDown(kPadButton4)) keys[DIK_RETURN] = 1; // START: 決定
            if (Controller::IsDown(kPadButton5)) keys[DIK_ESCAPE] = 1; // BACK: 終了
        }

        // 落雷は突進攻撃の一部。毎フレーム表示時間を減らす。
        for (int i = 0; i < kRushLightningCount; ++i) {
            if (lightningTimer[i] > 0) --lightningTimer[i];
        }
        if (currentScene != SCENE_BOSS) {
            for (int i = 0; i < kRushLightningCount; ++i) lightningTimer[i] = 0;
            rushLightningReady = false;
        }

        // 更新処理と描画処理の両方から参照するため、このスコープで宣言する。
        bool justWindow = false;

        ///
        /// ↓更新処理ここから
        ///

        // Enterキーを押した瞬間だけシーンを切り替える
        bool enter = preKeys[DIK_RETURN] == 0 && keys[DIK_RETURN] != 0;
        bool up = preKeys[DIK_W] == 0 && keys[DIK_W] != 0;
        bool down = preKeys[DIK_S] == 0 && keys[DIK_S] != 0;

        if (currentScene == SCENE_TITLE) {
            // W/Sキーでタイトルのボタンを選択
            if (up || down) titleSelection = 1 - titleSelection;
            if (enter) {
                if (titleSelection == 0) {
                    ResetBossBattle(player, boss, bullets);
                    counterEffect = {};
                    counterText = {};
                    counterCooldown = 0;
                    bossFlash = 0;
                    hitStopTimer = 0;
                    slowTimer = 0;
                    gameFrameAccumulator = 0.0f;
                    shakeTimer = 0;
                    shakeTotalFrames = kCounterShakeFrames;
                    shakeMaxPower = kCounterShakePower;
                    specialFlashTimer = 0;
                    specialImpactTimer = 0;
                    shakeX = shakeY = 0;
                    dashRequest = false;
                    jumpRequest = false;
                    dashTimer = 0;
                    dashDodgeEffect = 0;
                    dashCooldown = 0;
                    dashDirX = dashDirY = 0.0f;
                    dashWave = {};
                    for (int i = 0; i < kDashAfterimageCount; ++i) dashImages[i] = {};
                    dashImageIndex = 0;
                    swordSwingTimer = 0;
                    swordRequest = false;
                    normalSwordRequest = false;
                    normalSwordActive = false;
                    swordHitBoss = false;
                    swordHitFlashTimer = 0;
                    counterWindowTimer = 0;
                    counterArmed = false;
                    breath = {};
                    breathSecond = {};
                    charge = {};
                    rushLightningReady = false;
                    for (int i = 0; i < kRushLightningCount; ++i) {
                        lightningTimer[i] = 0;
                        lightningHit[i] = false;
                    }
                    wire = {};
                    wireWait = 0;
                    nextAttackWait = kAttackWaitMin;
                    lastAttack = -1;
                    attackInProgress = false;
                    wireEscapeEffect = 0;
                    giantChargeTimer = 0;
                    currentScene = SCENE_BOSS;
                }
                else currentScene = SCENE_CREDITS;
            }
        }
        else if (currentScene == SCENE_BOSS) {
            // 演出はヒットストップ・スロー中も実時間で進める
            if (dashCooldown > 0) --dashCooldown;
            if (dashDodgeEffect > 0) --dashDodgeEffect;
            if (dashWave.timer > 0) --dashWave.timer;
            for (int i = 0; i < kDashAfterimageCount; ++i) {
                if (dashImages[i].timer > 0) --dashImages[i].timer;
            }
            if (counterCooldown > 0) --counterCooldown;
            if (counterEffect.timer > 0) --counterEffect.timer;
            if (counterText.timer > 0) --counterText.timer;
            if (bossFlash > 0) --bossFlash;
            if (swordHitFlashTimer > 0) --swordHitFlashTimer;
            if (shakeTimer > 0) --shakeTimer;
            if (specialFlashTimer > 0) --specialFlashTimer;
            if (specialImpactTimer > 0) --specialImpactTimer;
            if (wireEscapeEffect > 0) --wireEscapeEffect;
            if (wire.flashTimer > 0) --wire.flashTimer;
            if (charge.hitFlash > 0) --charge.hitFlash;
            if (charge.dustTimer > 0) --charge.dustTimer;
            if (charge.downHitFlash > 0) --charge.downHitFlash;

            // 攻撃が完全に終わるまで次の攻撃を選ばない。
            // 反射された通常弾も消えるまで待つので攻撃が重ならない。
            bool bulletRemaining = false;
            for (int i = 0; i < kMaxBullets; ++i) {
                if (bullets[i].active) { bulletRemaining = true; break; }
            }
            const bool bossBusy = wire.phase != WIRE_IDLE ||
                breath.phase != BREATH_IDLE || breathSecond.phase != BREATH_IDLE || charge.phase != CHARGE_IDLE || bulletRemaining || giantChargeTimer > 0;
            if (bossBusy) {
                attackInProgress = true;
            }
            else if (attackInProgress) {
                attackInProgress = false;
                nextAttackWait = kAttackWaitMin + rand() % kAttackWaitRandom;
            }
            else if (nextAttackWait > 0) {
                --nextAttackWait;
            }
            else {
                // 直前と同じ攻撃は選ばない。4種類をランダムに切り替える。
                int choice = rand() % 4;
                if (choice == lastAttack) choice = (choice + 1 + rand() % 3) % 4;
                lastAttack = choice;
                attackInProgress = true;
                if (choice == 3) {
                    // 毎回ランダムな角度で左画面外から突進する（約-18～+18度）。
                    // プレイヤーの予告開始時の位置を通る軌道に固定する。
                    charge = {};
                    charge.phase = CHARGE_WARNING;
                    // 画面の地面に3か所の着弾位置をランダム決定。
                    // 互いに近すぎる位置を避けて回避できる隙間を作る。
                    for (int i = 0; i < kRushLightningCount; ++i) {
                        float candidate = 170.0f;
                        for (int attempt = 0; attempt < 30; ++attempt) {
                            candidate = 170.0f + (float)(rand() % (kWidth - 340));
                            bool separated = true;
                            for (int j = 0; j < i; ++j) {
                                if (fabsf(candidate - lightningX[j]) < 230.0f) separated = false;
                            }
                            if (separated) break;
                        }
                        lightningX[i] = candidate;
                        lightningTimer[i] = 0;
                        lightningHit[i] = false;
                    }
                    rushLightningReady = true;
                    const float degrees = (float)(rand() % 37 - 18);
                    charge.angle = degrees * 3.14159265f / 180.0f;
                    charge.vx = cosf(charge.angle) * kChargeSpeed;
                    charge.vy = sinf(charge.angle) * kChargeSpeed;
                    charge.targetY = player.y + 24.0f;
                    charge.x = -(kChargeWidth + 30.0f);
                    // プレイヤーのX位置で指定した高さになるよう、開始位置を逆算。
                    charge.y = charge.targetY - kChargeHeight * 0.5f
                        - (player.x + 18.0f - charge.x) * (charge.vy / charge.vx);
                }
                else if (choice == 1) {
                    wire.phase = WIRE_WARNING;
                    wire.timer = 0;
                    // 予告開始時のプレイヤーの真下を出現地点として固定する。
                    wire.groundX = player.x + 18.0f;
                    wire.groundY = kGroundY + 16.0f;
                    wire.targetX = player.x + 18.0f;
                    wire.targetY = player.y + 24.0f;
                }
                else if (choice == 2) {
                    // ブレスを画面上部の左右から2本同時発射する。
                    // それぞれ地面のランダムな位置を狙い、予告開始時に着弾位置を固定する。
                    const float breathStartY = 70.0f;
                    const float breathStartLeftX = 110.0f;
                    const float breathStartRightX = (float)kWidth - 110.0f;
                    const int targetMargin = 160;
                    const int targetRange = kWidth - targetMargin * 2;
                    const float firstTargetX = (float)(targetMargin + rand() % targetRange);
                    float secondTargetX = (float)(targetMargin + rand() % targetRange);
                    // 2本の着弾地点が近すぎる場合は再抽選する。
                    for (int retry = 0; retry < 12 && fabsf(secondTargetX - firstTargetX) < 280.0f; ++retry) {
                        secondTargetX = (float)(targetMargin + rand() % targetRange);
                    }
                    for (int i = 0; i < 2; ++i) {
                        DragonBreath& currentBreath = (i == 0) ? breath : breathSecond;
                        currentBreath = {};
                        currentBreath.phase = BREATH_WARNING;
                        currentBreath.startX = (i == 0) ? breathStartLeftX : breathStartRightX;
                        currentBreath.startY = breathStartY;
                        const float targetX = (i == 0) ? firstTargetX : secondTargetX;
                        const float targetY = kGroundY;
                        const float dx = targetX - currentBreath.startX;
                        const float dy = targetY - currentBreath.startY;
                        float dist = sqrtf(dx * dx + dy * dy);
                        if (dist < 0.001f) dist = 1.0f;
                        currentBreath.dirX = dx / dist;
                        currentBreath.dirY = dy / dist;
                    }
                }
                else {
                    // 通常弾の代わりに、巨大魔力球の溜め攻撃を開始する。
                    giantChargeTimer = 1;
                }
            }

            // 巨大魔力球：チャージ後に5個同時発射する。
            // 発射方向を扇状にずらし、最初の30ゲームフレームだけ緩やかに追尾する。
            if (giantChargeTimer > 0) {
                ++giantChargeTimer;
                if (giantChargeTimer >= kGiantChargeFrames) {
                    giantChargeTimer = 0;
                    const float sx = boss.x + kDragonMouthOffsetX;
                    const float sy = boss.y + kDragonMouthY;
                    const float dx = player.x + 18.0f - sx;
                    const float dy = player.y + 24.0f - sy;
                    const float baseAngle = atan2f(dy, dx);
                    int spawned = 0;
                    for (int i = 0; i < kMaxBullets && spawned < kGiantOrbCount; ++i) {
                        if (bullets[i].active) continue;
                        // 5発を約±24度の扇形に発射。出現位置も少しずらして重なりを減らす。
                        const float offset = (float)(spawned - 2);
                        const float angle = baseAngle + offset * 0.21f;
                        const float vx = cosf(angle) * kGiantOrbSpeed;
                        const float vy = sinf(angle) * kGiantOrbSpeed;
                        bullets[i] = {};
                        bullets[i].x = sx + cosf(baseAngle + 1.5707963f) * offset * 22.0f;
                        bullets[i].y = sy + sinf(baseAngle + 1.5707963f) * offset * 22.0f;
                        bullets[i].vx = vx;
                        bullets[i].vy = vy;
                        bullets[i].active = true;
                        bullets[i].giant = true;
                        bullets[i].reflectLevel = 0;
                        bullets[i].homingTimer = kGiantHomingFrames;
                        bullets[i].appearanceTimer = 0;
                        ++spawned;
                    }
                }
            }

            // 龍の画面外突進。実フレーム更新で予告時間を安定させる。
            if (charge.phase == CHARGE_WARNING) {
                if (++charge.timer >= kChargeWarningFrames) {
                    charge.phase = CHARGE_LEAVE;
                    charge.timer = 0;
                }
            }
            else if (charge.phase == CHARGE_LEAVE) {
                if (++charge.timer >= kChargeLeaveFrames) {
                    charge.phase = CHARGE_RUSH;
                    charge.timer = 0;
                    charge.x = -(kChargeWidth + 30.0f);
                    // 開始位置は予告時に決めた斜めの軌道を維持する。
                }
            }
            else if (charge.phase == CHARGE_RUSH) {
                ++charge.timer;
                // 突進開始とともに、3本の落雷を12F間隔で落とす。
                if (rushLightningReady) {
                    for (int i = 0; i < kRushLightningCount; ++i) {
                        if (charge.timer == 1 + i * kRushLightningInterval) {
                            lightningTimer[i] = kRushLightningDuration;
                        }
                        // 雷は地面付近でのみ当たり判定を持つ。
                        if (lightningTimer[i] > 0 && !lightningHit[i] && dashTimer <= 0 &&
                            IsHit(player.x, player.y, 36.0f, 48.0f,
                                lightningX[i] - 42.0f, kGroundY - 190.0f, 84.0f, 190.0f)) {
                            lightningHit[i] = true;
                            charge.hitFlash = 16; // 既存仕様と同様、被弾演出のみ
                        }
                    }
                }
                const float previousX = charge.x;
                const float previousY = charge.y;
                charge.x += charge.vx;
                charge.y += charge.vy;
                // 斜め移動でも高速すり抜けを防ぐため4分割で判定する。
                bool chargeHit = false;
                for (int step = 1; step <= 4; ++step) {
                    float t = (float)step / 4.0f;
                    float hitX = previousX + charge.vx * t;
                    float hitY = previousY + charge.vy * t;
                    if (IsHit(player.x, player.y, 36.0f, 48.0f,
                        hitX, hitY, kChargeWidth, kChargeHeight)) {
                        chargeHit = true;
                        break;
                    }
                }
                if (!charge.hitOnce && chargeHit) {
                    charge.hitOnce = true;
                    // 突進へのジャストパリィはダッシュ回避より優先する。
                    // Jを押してからの受付時間内、または接触した瞬間のJで成功。
                    if ((counterArmed && counterWindowTimer > 0) ||
                        (keys[DIK_J] && !preKeys[DIK_J] && counterCooldown == 0)) {
                        charge.phase = CHARGE_FALL;
                        charge.timer = 0;
                        charge.fallVelocityY = -9.0f; // 一度上に弾かれてから墜落
                        charge.fallSpin = charge.angle;
                        charge.hitFlash = 18;
                        counterEffect = { player.x + 18.0f, player.y + 24.0f, 30, true };
                        StartCounterText(counterText, COUNTER_TEXT_BREAK,
                            player.x + 18.0f, player.y - 48.0f);
                        hitStopTimer = kDashCounterHitStopFrames;
                        slowTimer = kDashCounterSlowFrames;
                        shakeTotalFrames = kDashCounterShakeFrames;
                        shakeMaxPower = kDashCounterShakePower;
                        shakeTimer = shakeTotalFrames;
                        gameFrameAccumulator = 0.0f;
                        counterArmed = false;
                        counterWindowTimer = 0;
                        counterCooldown = kCounterRetryCooldown;
                        swordSwingTimer = kSwordSwingFrames;
                        swordRequest = false;
                        normalSwordActive = false;
                    }
                    else if (dashTimer > 0) {
                        dashDodgeEffect = 20;
                    }
                    else {
                        // 他の攻撃と同様、テスト中は死亡させず被弾演出のみ。
                        charge.hitFlash = 20;
                    }
                }
                if (charge.x > kWidth + 30.0f || charge.y > kHeight + kChargeHeight + 100.0f ||
                    charge.y < -kChargeHeight - 350.0f) {
                    charge.phase = CHARGE_RECOVERY;
                    charge.timer = 0;
                }
            }
            else if (charge.phase == CHARGE_FALL) {
                // パリィされた龍は勢いを失u
                ++charge.timer;
                charge.fallVelocityY += kChargeFallGravity;
                charge.y += charge.fallVelocityY;
                charge.x += charge.vx * 0.12f; // 慣性で少しだけ前進
                charge.fallSpin += 0.13f;
                if (charge.x > kWidth - kChargeWidth) charge.x = kWidth - kChargeWidth;
                if (charge.x < 0.0f) charge.x = 0.0f;
                if (charge.y >= kGroundY - kChargeHeight) {
                    charge.y = kGroundY - kChargeHeight;
                    charge.phase = CHARGE_DOWN;
                    charge.timer = 0;
                    charge.fallVelocityY = 0.0f;
                    charge.fallSpin = 0.0f;
                    charge.dustTimer = 36;
                    shakeTotalFrames = 20;
                    shakeMaxPower = 19.0f;
                    shakeTimer = 20;
                }
            }
            else if (charge.phase == CHARGE_DOWN) {
                // ダウン中は他の攻撃を出さず、240実フレーム追撃可能。
                if (++charge.timer >= kChargeDownFrames) {
                    charge.phase = CHARGE_GETUP;
                    charge.timer = 0;
                }
            }
            else if (charge.phase == CHARGE_GETUP) {
                // 起き上がりの間も新しい攻撃を禁止。
                if (++charge.timer >= kChargeGetupFrames) {
                    charge.phase = CHARGE_RECOVERY;
                    charge.timer = 0;
                }
            }
            else if (charge.phase == CHARGE_RECOVERY) {
                if (++charge.timer >= kChargeRecoveryFrames) {
                    charge.phase = CHARGE_IDLE;
                    charge.timer = 0;
                }
            }

            // 2本のブレスをそれぞれ更新し、当たり判定も個別に計算する。
            for (int breathIndex = 0; breathIndex < 2; ++breathIndex) {
                DragonBreath& currentBreath = (breathIndex == 0) ? breath : breathSecond;
                // 龍ブレスは予告開始時のランダムな位置に向けて照準を固定する。
                // 発射後にプレイヤーを追尾しないため、ジャンプや空中ダッシュで回避できる。
                if (currentBreath.hitFlash > 0) --currentBreath.hitFlash;
                if (currentBreath.hitCooldown > 0) --currentBreath.hitCooldown;
                if (currentBreath.phase == BREATH_WARNING) {
                    if (++currentBreath.timer >= kBreathWarningFrames) {
                        currentBreath.phase = BREATH_EXTEND;
                        currentBreath.timer = 0;
                    }
                }
                else if (currentBreath.phase == BREATH_EXTEND) {
                    ++currentBreath.timer;
                    float progress = (float)currentBreath.timer / (float)kBreathExtendFrames;
                    if (progress > 1.0f) progress = 1.0f;
                    currentBreath.length = 2100.0f * progress;
                    if (currentBreath.timer >= kBreathExtendFrames) {
                        currentBreath.phase = BREATH_HOLD;
                        currentBreath.timer = 0;
                        currentBreath.length = 2100.0f;
                    }
                }
                else if (currentBreath.phase == BREATH_HOLD) {
                    if (++currentBreath.timer >= kBreathHoldFrames) {
                        currentBreath.phase = BREATH_RECOVERY;
                        currentBreath.timer = 0;
                    }
                }
                else if (currentBreath.phase == BREATH_RECOVERY) {
                    if (++currentBreath.timer >= kBreathRecoveryFrames) {
                        currentBreath.phase = BREATH_IDLE;
                        currentBreath.timer = 0;
                    }
                }
                // 当たり判定：プレイヤー中心とブレス線分の最短距離で判定。
                // 元の無敵テスト仕様に合わせて、ヒット演出だけを表示する。
                if ((currentBreath.phase == BREATH_EXTEND || currentBreath.phase == BREATH_HOLD) &&
                    currentBreath.hitCooldown == 0 && currentBreath.length > 0.0f) {
                    float px = player.x + 18.0f - currentBreath.startX;
                    float py = player.y + 24.0f - currentBreath.startY;
                    float along = px * currentBreath.dirX + py * currentBreath.dirY;
                    if (along < 0.0f) along = 0.0f;
                    if (along > currentBreath.length) along = currentBreath.length;
                    float nearX = currentBreath.startX + currentBreath.dirX * along;
                    float nearY = currentBreath.startY + currentBreath.dirY * along;
                    float diffX = player.x + 18.0f - nearX;
                    float diffY = player.y + 24.0f - nearY;
                    const float hitRadius = kBreathRadius + 22.0f;
                    if (diffX * diffX + diffY * diffY <= hitRadius * hitRadius && dashTimer > 0) {
                        dashDodgeEffect = 20; // ブレスをダッシュで回避
                        currentBreath.hitCooldown = 12;
                    }
                    else if (diffX * diffX + diffY * diffY <= hitRadius * hitRadius) {
                        currentBreath.hitFlash = 14;
                        currentBreath.hitCooldown = 45;
                    }
                }

            }

            const bool justPressedJ = keys[DIK_J] && !preKeys[DIK_J];
            // しっぽは演出中も実フレームで進める）。
            // 発射方向は予告終了時のプレイヤー中心に固定する。
            if (wire.phase == WIRE_WARNING) {
                if (++wire.timer >= kWireWarningFrames) {
                    wire.phase = WIRE_FLYING;
                    wire.timer = 0;
                    // 龍のしっぽは地面から出て、予告終了時のプレイヤー位置へ伸びる。
                    wire.x = wire.groundX;
                    wire.y = wire.groundY;
                    wire.targetX = player.x + 18.0f;
                    wire.targetY = player.y + 24.0f;
                    float dx = wire.targetX - wire.x;
                    float dy = wire.targetY - wire.y;
                    float len = sqrtf(dx * dx + dy * dy);
                    if (len < 0.001f) len = 1.0f;
                    wire.vx = dx / len * kWireSpeed;
                    wire.vy = dy / len * kWireSpeed;
                }
            }
            else if (wire.phase == WIRE_FLYING) {
                ++wire.timer;
                // 高速ワイヤーのすり抜け防止：移動線分を小刻みに判定。
                bool caught = false;
                for (int step = 0; step < 4; ++step) {
                    wire.x += wire.vx * 0.25f;
                    wire.y += wire.vy * 0.25f;
                    if (IsHit(player.x, player.y, 36.0f, 48.0f,
                        wire.x - 7.0f, wire.y - 7.0f, 14.0f, 14.0f)) {
                        caught = true;
                        break;
                    }
                }
                if (caught && ((counterArmed && counterWindowTimer > 0) ||
                    (justPressedJ && counterCooldown == 0))) {
                    // 掴まれる直前のカウンター成功：拘束させずにしっぽを弾く。
                    wire.phase = WIRE_RECOVERY;
                    wire.timer = 0;
                    wire.flashTimer = 16;
                    wireEscapeEffect = 30;
                    wire.escaped = true;
                    swordSwingTimer = kSwordSwingFrames;
                    swordRequest = false;
                    normalSwordActive = false;
                    counterArmed = false;
                    counterWindowTimer = 0;
                    counterCooldown = kCounterRetryCooldown;
                    counterEffect = { wire.x, wire.y, 30, true };
                    StartCounterText(counterText, COUNTER_TEXT_BREAK, player.x + 18.0f, player.y - 48.0f);
                    hitStopTimer = 5;
                    slowTimer = 18;
                    shakeTotalFrames = kDashCounterShakeFrames;
                    shakeMaxPower = kDashCounterShakePower;
                    shakeTimer = shakeTotalFrames;
                    gameFrameAccumulator = 0.0f;
                }
                else if (caught && dashTimer > 0) {
                    // ダッシュ中はしっぽを回避、拘束されない。
                    dashDodgeEffect = 20;
                    wire.phase = WIRE_RECOVERY;
                    wire.timer = 0;
                    wire.flashTimer = 12;
                }
                else if (caught) {
                    wire.phase = WIRE_BOUND;
                    wire.timer = 0;
                    wire.x = player.x + 18.0f;
                    wire.y = player.y + 24.0f;
                    player.velocityY = 0.0f;
                    dashTimer = 0;
                    dashRequest = false;
                    jumpRequest = false;
                    swordRequest = false;
                    counterArmed = false;
                    counterWindowTimer = 0;
                }
                else if (wire.timer > 90 || wire.x < -40 || wire.x > kWidth + 40 ||
                    wire.y < -40 || wire.y > kHeight + 40) {
                    wire.phase = WIRE_RECOVERY;
                    wire.timer = 0;
                }
            }
            else if (wire.phase == WIRE_BOUND) {
                ++wire.timer;
                // しっぽが地面へ引きずり込む。画面と地面からはみ出さない。
                float tx = wire.groundX - 18.0f;
                float ty = kGroundY - 48.0f;
                float dx = tx - (player.x + 18.0f);
                float dy = ty - (player.y + 24.0f);
                float len = sqrtf(dx * dx + dy * dy);
                if (len > 0.001f) {
                    float speed = len < 4.0f ? len : 4.0f;
                    player.x += dx / len * speed;
                    player.y += dy / len * speed;
                }
                if (player.x < 0.0f) player.x = 0.0f;
                if (player.x > kWidth - 36.0f) player.x = kWidth - 36.0f;
                if (player.y < 0.0f) player.y = 0.0f;
                if (player.y > kGroundY - 48.0f) player.y = kGroundY - 48.0f;
                wire.x = player.x + 18.0f;
                wire.y = player.y + 24.0f;
                player.velocityY = 0.0f;
                player.onGround = (player.y >= kGroundY - 48.0f);
                if (wire.timer >= kWireMaxBoundFrames) {
                    wire.phase = WIRE_RECOVERY;
                    wire.timer = 0;
                }
            }
            else if (wire.phase == WIRE_RECOVERY) {
                if (++wire.timer >= 35) {
                    wire.phase = WIRE_IDLE;
                    wire.timer = 0;
                }
            }
            // Jで剣を振り、短いカウンター受付を開始する。
            // しっぽが触れる直前に押すと、拘束される前に切断できる。
            // 捕まった後はJで脱出できない（今回は接触時カウンター方式）。
            if (justPressedJ && wire.phase != WIRE_BOUND && wireEscapeEffect == 0) {
                swordRequest = true;
                normalSwordRequest = false;
                if (counterCooldown == 0) {
                    counterArmed = true;
                    counterWindowTimer = kJustCounterWindow;
                    counterCooldown = kCounterRetryCooldown;
                }
            }
            justWindow = wire.phase != WIRE_BOUND && counterArmed && counterWindowTimer > 0;
            // ヒットストップ中は判定時間を減らさない。
            if (hitStopTimer == 0 && counterWindowTimer > 0) {
                --counterWindowTimer;
                if (counterWindowTimer == 0) counterArmed = false;
            }
            // 演出タイマーは毎フレーム進めるが、ゲーム本体の更新だけを止めたり間引いたりする。
            // ヒットストップ中は物理更新0回、スロー中は4フレームに1回、通常時は毎フレーム。
            float gameSpeed = 1.0f;
            if (hitStopTimer > 0) {
                gameSpeed = 0.0f;
                --hitStopTimer;
            }
            else if (slowTimer > 0) {
                gameSpeed = kCounterSlowScale;
                --slowTimer;
            }
            // Shift押下を更新フレームまで保持し、スロー中の入力抜けを防ぐ。
            if (keys[DIK_LSHIFT] && !preKeys[DIK_LSHIFT]) dashRequest = true;
            if (!keys[DIK_LSHIFT]) dashRequest = false;
            // SPACEを押した瞬間にジャンプ予約（スロー・ヒットストップ中の入力抜け対策）。
            if (wire.phase != WIRE_BOUND && keys[DIK_SPACE] && !preKeys[DIK_SPACE]) jumpRequest = true;
            // Hを押した瞬間に剣を振る。ヒットストップ中の入力も予約する。
            if (wire.phase != WIRE_BOUND && keys[DIK_H] && !preKeys[DIK_H]) {
                swordRequest = true;
                normalSwordRequest = true;
            }
            gameFrameAccumulator += gameSpeed;
            bool updateGame = false;
            if (gameFrameAccumulator >= 1.0f) {
                gameFrameAccumulator -= 1.0f;
                updateGame = true;
            }

            if (updateGame && wire.phase != WIRE_BOUND) {
                if (swordSwingTimer > 0) --swordSwingTimer;
                if (swordSwingTimer == 0) normalSwordActive = false;
                if (swordRequest) {
                    swordSwingTimer = kSwordSwingFrames;
                    normalSwordActive = normalSwordRequest;
                    swordHitBoss = false;
                }
                swordRequest = false;
                normalSwordRequest = false;
                // A/Dで左右移動。W/Sは空中ダッシュの方向指定にだけ使用する。
                float moveX = 0.0f;
                if (keys[DIK_A]) moveX -= 1.0f;
                if (keys[DIK_D]) moveX += 1.0f;
                if (moveX < 0.0f) player.direction = -1;
                else if (moveX > 0.0f) player.direction = 1;

                // 地面にいるときだけジャンプ可能（空中での連続ジャンプを防ぐ）。
                if (jumpRequest && player.onGround) {
                    player.velocityY = kJumpPower;
                    player.onGround = false;
                }
                jumpRequest = false;

                // Shiftダッシュ：地上では左右、空中ではW/Sで上下にも移動できる。
                // 空中でW+ Dなどを同時押しすると斜めダッシュ（速度は正規化）。
                if (dashRequest && dashCooldown == 0 && dashTimer == 0) {
                    float inputDashX = moveX;
                    float inputDashY = 0.0f;
                    if (!player.onGround) {
                        if (keys[DIK_W]) inputDashY -= 1.0f;
                        if (keys[DIK_S]) inputDashY += 1.0f;
                    }
                    // 方向キーなしなら現在向いている左右へダッシュ。
                    if (inputDashX == 0.0f && inputDashY == 0.0f) {
                        inputDashX = (float)player.direction;
                    }
                    float dashLength = sqrtf(inputDashX * inputDashX + inputDashY * inputDashY);
                    dashDirX = inputDashX / dashLength;
                    dashDirY = inputDashY / dashLength;
                    dashTimer = kDashDuration;
                    dashCooldown = kDashCooldown;
                    dashWave = { player.x + 18.0f, player.y + 24.0f, kDashWaveFrames };
                    // 空中ダッシュ開始時に落下速度をリセット。
                    if (!player.onGround) player.velocityY = 0.0f;
                }
                dashRequest = false;
                if (dashTimer > 0) {
                    player.x += dashDirX * kDashSpeed;
                    player.y += dashDirY * kDashSpeed;
                    --dashTimer;
                    // ダッシュ中は重力を止めて、指定した方向へまっすぐ進む。
                    player.velocityY = 0.0f;
                    dashImages[dashImageIndex] = { player.x, player.y, 14 };
                    dashImageIndex = (dashImageIndex + 1) % kDashAfterimageCount;
                }
                else {
                    player.x += moveX * kMoveSpeed;
                    player.velocityY += kGravity;
                    player.y += player.velocityY;
                }

                // 地面への着地処理。
                if (player.y >= kGroundY - 48.0f) {
                    player.y = kGroundY - 48.0f;
                    player.velocityY = 0.0f;
                    player.onGround = true;
                    // 地面に当たったら下向きダッシュを終了する。
                    if (dashDirY > 0.0f) dashTimer = 0;
                }
                else {
                    player.onGround = false;
                }
                // プレイヤーが画面外に出ないように制限。
                if (player.x < 0.0f) player.x = 0.0f;
                if (player.x > kWidth - 36.0f) player.x = kWidth - 36.0f;
                if (player.y < 0.0f) {
                    player.y = 0.0f;
                    if (player.velocityY < 0.0f) player.velocityY = 0.0f;
                }

                // H通常攻撃：プレイヤーの向いている側に100x100の判定を置く。
                // 斬撃の中盤だけ有効。Jカウンターの剣振りでは判定しない。
                const bool swordHitActive = normalSwordActive &&
                    swordSwingTimer <= kSwordHitStart && swordSwingTimer >= kSwordHitEnd;
                if (swordHitActive && !swordHitBoss) {
                    float hitX = player.direction == 1
                        ? player.x + 18.0f : player.x + 18.0f - kSwordHitWidth;
                    float hitY = player.y + 24.0f - kSwordHitHeight * 0.5f;
                    // ダウン中の龍は、画面右端の通常ボスではなく落下した位置で判定。
                    const bool hitDownDragon = charge.phase == CHARGE_DOWN &&
                        IsHit(hitX, hitY, kSwordHitWidth, kSwordHitHeight,
                            charge.x, charge.y, kChargeWidth, kChargeHeight);
                    const bool hitStandingDragon = charge.phase != CHARGE_LEAVE &&
                        charge.phase != CHARGE_RUSH && charge.phase != CHARGE_FALL &&
                        charge.phase != CHARGE_DOWN && charge.phase != CHARGE_GETUP &&
                        IsHit(hitX, hitY, kSwordHitWidth, kSwordHitHeight,
                            boss.x, boss.y, kBossWidth, kBossHeight);
                    if (hitDownDragon || hitStandingDragon) {
                        if (hitDownDragon) {
                            charge.downHitFlash = 12;
                            counterEffect = { charge.x + kChargeWidth * 0.5f,
                                charge.y + kChargeHeight * 0.5f, 30, false };
                        }
                        // ダウン中は通常の3倍のダメージ
                        boss.hp -= hitDownDragon ? 30 : 10;
                        if (boss.hp < 0) boss.hp = 0;
                        swordHitBoss = true;
                        swordHitFlashTimer = 12;
                        bossFlash = 12; // 試作用：命中するとボスが白く光る
                    }
                }

                // 通常弾の発射は共通スケジューラで管理する（重複発射しない）。
                // 弾の移動とプレイヤーへの当たり判定
                for (int i = 0; i < kMaxBullets; ++i) {
                    Bullet& b = bullets[i];
                    if (!b.active) continue;
                    // 反射弾だけ、移動前の位置を残像として保存する。
                    // ゲーム更新時だけ記録するのでスロー中に残像が増えすぎない。
                    if (b.reflected) {
                        for (int j = kTrailLength - 1; j > 0; --j) {
                            b.trail[j] = b.trail[j - 1];
                        }
                        b.trail[0] = { b.x, b.y };
                        if (b.trailCount < kTrailLength) ++b.trailCount;
                    }
                    // 未反射の巨大魔力球だけ、短時間ゆるくプレイヤーへ向きを変える。
                    // 反射した弾は従来どおりボスへ直進する。
                    if (b.giant && !b.reflected && b.homingTimer > 0) {
                        --b.homingTimer;
                        const float dx = player.x + 18.0f - b.x;
                        const float dy = player.y + 24.0f - b.y;
                        const float dist = sqrtf(dx * dx + dy * dy);
                        if (dist > 0.001f) {
                            // 現在の進行方向と目標方向を混ぜて、急に曲がらないようにする。
                            const float targetVX = dx / dist * kGiantOrbSpeed;
                            const float targetVY = dy / dist * kGiantOrbSpeed;
                            b.vx += (targetVX - b.vx) * kGiantHomingTurn;
                            b.vy += (targetVY - b.vy) * kGiantHomingTurn;
                            const float speed = sqrtf(b.vx * b.vx + b.vy * b.vy);
                            if (speed > 0.001f) {
                                b.vx = b.vx / speed * kGiantOrbSpeed;
                                b.vy = b.vy / speed * kGiantOrbSpeed;
                            }
                        }
                    }
                    b.x += b.vx;
                    b.y += b.vy;
                    // ジャスト判定中はプレイヤー中心から90px以内に入った弾を反射。
                    // 成功した瞬間に、既存の強いシェイク・停止・スローを開始する。
                    if (!b.reflected && justWindow) {
                        float dx = b.x - (player.x + 18.0f);
                        float dy = b.y - (player.y + 24.0f);
                        if (dx * dx + dy * dy <= (b.giant ? 120.0f * 120.0f : 90.0f * 90.0f)) {
                            // Shiftキーではなく「実際にダッシュしているか」で判定する。
                            const bool dashJustCounter = dashTimer > 0;
                            counterEffect = { b.x, b.y, 30, dashJustCounter };
                            StartCounterText(counterText,
                                dashJustCounter ? COUNTER_TEXT_PERFECT : COUNTER_TEXT_PARRY,
                                player.x + 18.0f, player.y - 48.0f);
                            shakeTotalFrames = dashJustCounter ? kDashCounterShakeFrames : kCounterShakeFrames;
                            shakeMaxPower = dashJustCounter ? kDashCounterShakePower : kCounterShakePower;
                            shakeTimer = shakeTotalFrames;
                            hitStopTimer = dashJustCounter ? kDashCounterHitStopFrames : kCounterHitStopFrames;
                            slowTimer = dashJustCounter ? kDashCounterSlowFrames : kCounterSlowFrames;
                            if (dashJustCounter) specialFlashTimer = kDashCounterFlashFrames;
                            gameFrameAccumulator = 0.0f;
                            counterCooldown = kCounterRetryCooldown;
                            float toBossX = boss.x + kBossWidth * 0.5f - b.x;
                            float toBossY = boss.y + kBossWireOffsetY - b.y;
                            float length = sqrtf(toBossX * toBossX + toBossY * toBossY);
                            if (length < 0.001f) length = 1.0f;
                            const float reflectedSpeed = dashJustCounter ? kDashCounterBulletSpeed : kCounterBulletSpeed;
                            b.vx = toBossX / length * reflectedSpeed;
                            b.vy = toBossY / length * reflectedSpeed;
                            b.reflected = true;
                            b.dashReflected = dashJustCounter;
                            b.reflectLevel = dashJustCounter ? 3 : 2;
                            // 反射前の黄色い弾の軌跡は引き継がない
                            b.trailCount = 0;
                            // 1回の押下で複数の弾を反射しない
                            counterWindowTimer = 0;
                            swordSwingTimer = kSwordSwingFrames; // 成功時も剣を振る動きを強調
                            normalSwordActive = false; // Jのカウンターは通常攻撃扱いにしない
                            counterArmed = false;
                        }
                    }
                    // 通常パリィ：ジャスト受付終了後でも、剣を振っている間に
                    // 巨大魔力球が近くに来れば低速で押し返せる（強化演出なし）。
                    // 先にジャスト判定を行うため、ジャスト成功時はこちらには入らない。
                    if (b.active && b.giant && !b.reflected && !justWindow &&
                        swordSwingTimer > 0 && !normalSwordActive &&
                        counterWindowTimer == 0) {
                        float px = player.x + 18.0f;
                        float py = player.y + 24.0f;
                        float dx = b.x - px, dy = b.y - py;
                        float range = kGiantRadius + 48.0f;
                        if (dx * dx + dy * dy <= range * range) {
                            float bx = boss.x + kBossWidth * 0.5f - b.x;
                            float by = boss.y + kBossWireOffsetY - b.y;
                            float len = sqrtf(bx * bx + by * by);
                            if (len < 0.001f) len = 1.0f;
                            b.vx = bx / len * 6.0f;
                            b.vy = by / len * 6.0f;
                            b.reflected = true;
                            b.reflectLevel = 1;
                            b.trailCount = 0;
                            counterEffect = { b.x, b.y, 30, false };
                        }
                    }
                    const float margin = b.giant ? kGiantRadius : 20.0f;
                    if (b.x < -margin || b.x > kWidth + margin ||
                        b.y < -margin || b.y > kHeight + margin) {
                        b.active = false;
                        continue;
                    }
                    if (b.reflected && IsHit(boss.x, boss.y, kBossWidth, kBossHeight,
                        b.x - (b.giant ? kGiantRadius : 8.0f),
                        b.y - (b.giant ? kGiantRadius : 8.0f),
                        b.giant ? kGiantRadius * 2.0f : 16.0f,
                        b.giant ? kGiantRadius * 2.0f : 16.0f)) {
                        b.active = false;
                        // 反射の強さに応じてHPを減らす
                        boss.hp -= b.reflectLevel == 3 ? 100 : b.reflectLevel == 2 ? 50 : 20;
                        if (boss.hp < 0) boss.hp = 0;
                        bossFlash = b.giant ? (b.reflectLevel == 3 ? 32 : b.reflectLevel == 2 ? 22 : 10) : 12; // 反射の強さに応じて演出
                        if (b.dashReflected) {
                            specialImpactX = b.x;
                            specialImpactY = b.y;
                            specialImpactTimer = kDashCounterImpactFrames;
                        }
                        continue;
                    }
                    if (!b.reflected && IsHit(player.x, player.y, 36, 48,
                        b.x - (b.giant ? kGiantRadius : 8.0f),
                        b.y - (b.giant ? kGiantRadius : 8.0f),
                        b.giant ? kGiantRadius * 2.0f : 16.0f,
                        b.giant ? kGiantRadius * 2.0f : 16.0f)) {
                        if (dashTimer > 0) {
                            dashDodgeEffect = 20;
                            b.active = false;
                            continue;
                        }
                        b.active = false;
                        // テスト中はガードしていなくても死亡しない
                        // 弾だけ消してボス戦を続行する
                        continue;
                    }
                }
            }
            // 龍のHPが0になったらクリア画面へ
            if (boss.hp <= 0) currentScene = SCENE_CLEAR;
        }
        else if (enter) {
            switch (currentScene) {
            case SCENE_GAMEOVER: currentScene = SCENE_TITLE; titleSelection = 0; break;
            case SCENE_CLEAR:    currentScene = SCENE_TITLE; titleSelection = 0; break;
            case SCENE_CREDITS:  currentScene = SCENE_TITLE; titleSelection = 1; break;
            default: break;
            }
        }
        /// ↑更新処理ここまで
        ///

        ///
        /// ↓描画処理ここから
        ///

        // 描画位置だけを揺らす。プレイヤー・弾の座標や当たり判定は変更しない。
        shakeX = 0;
        shakeY = 0;
        if (currentScene == SCENE_BOSS && shakeTimer > 0) {
            float t = (float)shakeTimer / (float)shakeTotalFrames;
            float power = shakeMaxPower * t; // 強化カウンターでは20pxから減衰
            int step = shakeTotalFrames - shakeTimer;
            shakeX = (int)(sinf(step * 2.4f + 0.7f) * power);
            shakeY = (int)(cosf(step * 3.1f + 0.4f) * power);
        }

        // 龍がブレスを放っている間は、ダッシュジャストカウンターと同じ
        // 最大20pxの強い画面シェイクを持続させる。
        // 予告中・攻撃終了後は揺らさず、当たり判定の座標も変更しない。
        if (currentScene == SCENE_BOSS &&
            ((breath.phase == BREATH_EXTEND || breath.phase == BREATH_HOLD) ||
                (breathSecond.phase == BREATH_EXTEND || breathSecond.phase == BREATH_HOLD))) {
            const float breathShakePower = kDashCounterShakePower;
            const float tick = (float)breath.timer;
            const int breathShakeX = (int)(sinf(tick * 2.4f + 0.7f) * breathShakePower);
            const int breathShakeY = (int)(cosf(tick * 3.1f + 0.4f) * breathShakePower);
            // 他のカウンター演出中はそのシェイクを優先する。
            if (shakeTimer <= 0) {
                shakeX = breathShakeX;
                shakeY = breathShakeY;
            }
        }

        // 突進中は小さく揺らす（カウンターのシェイクを優先）。
        if (currentScene == SCENE_BOSS && charge.phase == CHARGE_RUSH && shakeTimer <= 0) {
            shakeX = (int)(sinf((float)charge.timer * 2.4f) * 7.0f);
            shakeY = (int)(cosf((float)charge.timer * 3.0f) * 5.0f);
        }

        // 全シーン共通の単色背景を描画する
        DrawBackground();
        // ボス戦の背景は実フレームで動かす（ヒットストップ中も流れる）。
        if (currentScene == SCENE_BOSS) {
            backgroundTime += 0.0166667f;
            cloudScroll = fmodf(cloudScroll + 0.45f, 1920.0f);
            mistScroll = fmodf(mistScroll + 1.2f, 1024.0f);
            Novice::SetBlendMode(kBlendModeNormal);
            // 山と夜空：1920x1080の原寸で固定描画。
            Novice::DrawSprite(0, 0, bgTexture, 1.0f, 1.0f, 0.0f, 0xFFFFFFFFu);
            // 月：350x350。淡い加算発光を重ねる。
            Novice::DrawSprite(680, 85, moonTexture, 0.85f, 0.85f, 0.0f, 0xFFFFFFFFu);
            Novice::SetBlendMode(kBlendModeAdd);
            Novice::DrawSprite(680, 85, moonTexture, 0.85f, 0.85f, 0.0f,
                WithAlpha(0xA5CFFF00u, 24 + (int)(8.0f * sinf(backgroundTime))));
            Novice::SetBlendMode(kBlendModeNormal);
            
        }


        // ボスの待機アニメーションを毎フレーム進める。
        // 攻撃中の処理や当たり判定とは独立した描画用タイマー。
        if (currentScene == SCENE_BOSS) {
            ++bossIdleTimer;
            if (bossIdleTimer >= kBossIdleFrameDuration) {
                bossIdleTimer = 0;
                bossIdleFrame = (bossIdleFrame + 1) % kBossIdleFrameCount;
            }
        }

        // 現在のシーンに応じた画面を描画する
        switch (currentScene) {
        case SCENE_TITLE:
            Novice::ScreenPrintf(928, 330, "TITLE");
            DrawMenuButton(800, 430, "GAME START", titleSelection == 0);
            DrawMenuButton(800, 520, "CREDITS", titleSelection == 1);
            Novice::ScreenPrintf(810, 640, "W / S : SELECT    ENTER : OK");
            break;
        case SCENE_BOSS: {
            Novice::ScreenPrintf(870, 60, "BOSS BATTLE");
            // ジャンプ用の地面を描画する。
            // 画像の地面部分（896～1080px）を当たり判定の高さ930pxに合わせる。
            // 下端まで150pxに縮め、画面シェイクにも追従させる。
            Novice::DrawQuad(0, (int)kGroundY + shakeY,
                kWidth, (int)kGroundY + shakeY,
                0, kHeight + shakeY,
                kWidth, kHeight + shakeY,
                0, 896, 1920, 184, groundTexture, 0xFFFFFFFFu);
            // 背景の光は地面の上、プレイヤーや攻撃より後ろに描画する。
            DrawBackgroundParticles(giantGlowFrame);
            // ダッシュ残像：古いものほど薄く消える
            for (int i = 0; i < kDashAfterimageCount; ++i) {
                const DashAfterimage& a = dashImages[i];
                if (a.timer <= 0) continue;
                int alpha = a.timer * 9;
                Novice::DrawBox((int)a.x + shakeX, (int)a.y + shakeY, 36, 48,
                    0.0f, WithAlpha(0x4BBFFF00u, alpha), kFillModeSolid);
            }
            // ダッシュ開始地点から広がる二重の衝撃波
            if (dashWave.timer > 0) {
                int elapsed = kDashWaveFrames - dashWave.timer;
                int cx = (int)dashWave.x + shakeX;
                int cy = (int)dashWave.y + shakeY;
                int alpha = (dashWave.timer * 190) / kDashWaveFrames;
                int radius = 12 + elapsed * 7;
                Novice::DrawEllipse(cx, cy, radius, radius, 0.0f,
                    WithAlpha(0x7DDFFF00u, alpha), kFillModeWireFrame);
                if (radius > 18) {
                    Novice::DrawEllipse(cx, cy, radius - 18, radius - 18, 0.0f,
                        WithAlpha(0xC5F7FF00u, alpha / 2), kFillModeWireFrame);
                }
                // 発動直後だけ白い閃光
                if (elapsed < 4) {
                    Novice::DrawEllipse(cx, cy, 22 + elapsed * 5, 22 + elapsed * 5,
                        0.0f, WithAlpha(0xFFFFFFFFu, 140 - elapsed * 35), kFillModeWireFrame);
                }
            }
            //////////////////////////////////////////////////////////////////////
            //////////////////////////////////////////////////////////////////////////
            ////bossの描画↓
            // 奥側のレイヤー：ボス本体を先に描画する。
            // 突進の移動・落下・ダウン・起き上がり中以外は待機アニメーションを表示。
            if (charge.phase != CHARGE_LEAVE && charge.phase != CHARGE_RUSH &&
                charge.phase != CHARGE_FALL && charge.phase != CHARGE_DOWN &&
                charge.phase != CHARGE_GETUP) {
                // 7680x1080の横長画像から1920x1080を切り出す。
                // 0, 1920, 3840, 5760 の順で表示してループする。
                // ※ 0.25fでは480px幅になるため、1920px表示には1.0fを使う。
                if (bossIdleTexture >= 0) {
                    Novice::DrawSpriteRect(
                        1490, 0,
                        bossIdleFrame * kBossIdleFrameWidth, 0,
                        kBossIdleFrameWidth, kBossIdleFrameHeight,
                        bossIdleTexture,
                        0.25f, 1.0f, 0.0f, 0xFFFFFFFFu); // 切り出した1920x1080を等倍描画
                }

            }

            // 突進中の龍はボスの代わりに横長のシルエットとして画面を横断する。
            if (charge.phase == CHARGE_WARNING || charge.phase == CHARGE_LEAVE) {
                // 突進方向と一致する斜めの予告帯。
                float slope = charge.vy / charge.vx;
                float startCenterY = charge.y + kChargeHeight * 0.5f;
                float yLeft = startCenterY + (0.0f - charge.x) * slope;
                float yRight = startCenterY + ((float)kWidth - charge.x) * slope;
                int pulse = charge.timer % 16;
                for (int offset = -1; offset <= 1; ++offset) {
                    int lineOffset = offset * (int)(kChargeHeight * 0.5f);
                    Novice::DrawLine(0, (int)yLeft + lineOffset + shakeY,
                        kWidth, (int)yRight + lineOffset + shakeY,
                        offset == 0 ? WithAlpha(0xFF454500u, 80 + pulse * 5) : 0xFFAB67FFu);
                }
                Novice::ScreenPrintf(500, 110, "DRAGON CHARGE WARNING!");
            }
            if (charge.phase == CHARGE_RUSH) {
                const float cx = charge.x + kChargeWidth * 0.5f + shakeX;
                const float cy = charge.y + kChargeHeight * 0.5f + shakeY;
                // 速度感のある残像（少数の画像で2000描画制限に配慮）。
                Novice::SetBlendMode(kBlendModeAdd);
                for (int i = 3; i >= 1; --i) {
                    DrawChargeImage(chargeTexture2,
                        cx - charge.vx / kChargeSpeed * i * 40.0f,
                        cy - charge.vy / kChargeSpeed * i * 40.0f,
                        charge.angle, WithAlpha(0xFFFFFF00u, 110 - i * 25));
                }
                // 本体は通常合成、発光画像は加算合成で同じ位置に重ねる。
                Novice::SetBlendMode(kBlendModeNormal);
                if (chargeTexture1 >= 0) {
                    DrawChargeImage(chargeTexture1, cx, cy, charge.angle, 0xFFFFFFFFu);
                }
                else {
                    Novice::DrawBox((int)charge.x + shakeX, (int)charge.y + shakeY,
                        (int)kChargeWidth, (int)kChargeHeight, charge.angle, 0x5FCDE4FFu, kFillModeSolid);
                }
                Novice::SetBlendMode(kBlendModeAdd);
                DrawChargeImage(chargeTexture2, cx, cy, charge.angle, 0xFFFFFFFFu);
                Novice::SetBlendMode(kBlendModeNormal);
            }
            // パリィ後の龍：回転落下 → 地面で240Fダウン → 起き上がり。
            if (charge.phase == CHARGE_FALL || charge.phase == CHARGE_DOWN ||
                charge.phase == CHARGE_GETUP) {
                const int cx = (int)charge.x + shakeX;
                const int cy = (int)charge.y + shakeY;
                float rotation = charge.phase == CHARGE_FALL ? charge.fallSpin : 0.0f;
                if (charge.phase == CHARGE_GETUP) {
                    // 起き上がるにつれて龍の姿が上へ持ち上がる。
                    rotation = 0.06f * sinf((float)charge.timer * 0.35f);
                }
                unsigned int dragonColor = (charge.downHitFlash > 0 || charge.hitFlash > 0)
                    ? 0xFFFFFFFFu : 0xAD3D4EFFu;
                Novice::DrawBox(cx, cy, (int)kChargeWidth, (int)kChargeHeight,
                    rotation, dragonColor, kFillModeSolid);
                Novice::DrawBox(cx + 12, cy + 14, (int)kChargeWidth - 24,
                    (int)kChargeHeight - 28, rotation, 0xED7780FFu, kFillModeSolid);
                if (charge.phase == CHARGE_DOWN) {
                    Novice::ScreenPrintf(cx + 65, cy - 28, "DOWN! %d", (kChargeDownFrames - charge.timer + 59) / 60);
                }
                // 着地の土煙：最大16個の楕円だけで描画負荷を抑える。
                if (charge.dustTimer > 0) {
                    float elapsed = (float)(36 - charge.dustTimer);
                    for (int i = 0; i < 16; ++i) {
                        float side = (i % 2 == 0 ? -1.0f : 1.0f);
                        float px = charge.x + kChargeWidth * 0.5f + side * (20.0f + elapsed * (2.0f + (i % 5)));
                        float py = kGroundY - 12.0f - sinf((float)i * 1.7f) * 13.0f - elapsed * (0.5f + (i % 3) * 0.3f);
                        int radius = 7 + i % 6 + (int)(elapsed * 0.3f);
                        Novice::DrawEllipse((int)px + shakeX, (int)py + shakeY,
                            radius, radius / 2 + 3, 0.0f,
                            WithAlpha(0xB6A69A00u, charge.dustTimer * 5), kFillModeSolid);
                    }
                }
            }
            if (charge.hitFlash > 0)
                Novice::ScreenPrintf(490, 235, "CHARGE HIT! (TEST INVINCIBLE)");
            if (swordHitFlashTimer > 0)
                Novice::ScreenPrintf(560, 190, "SWORD HIT!");

            // 龍のしっぽ：予告中は地面が赤く光り、飛翔中は地面から太い尾が伸びる。
            if (wire.phase == WIRE_WARNING) {
                int gx = (int)wire.groundX + shakeX;
                int gy = (int)kGroundY + shakeY;
                int pulse = (wire.timer % 12) * 2;
                Novice::DrawEllipse(gx, gy, 30 + pulse, 10 + pulse / 3,
                    0.0f, WithAlpha(0xFF5544FFu, 140), kFillModeSolid);
                Novice::DrawEllipse(gx, gy, 40 + pulse, 16 + pulse / 3,
                    0.0f, 0xFFAA55FFu, kFillModeWireFrame);
                Novice::ScreenPrintf(540, 125, "TAIL WARNING!");
            }
            if (wire.phase == WIRE_FLYING || wire.phase == WIRE_BOUND) {
                int ax = (int)wire.groundX + shakeX;
                int ay = (int)wire.groundY + shakeY;
                int wx = (int)wire.x + shakeX;
                int wy = (int)wire.y + shakeY;
                // 太い尾を重ね描きして龍のしっぽらしい厚みを出す。
                for (int offset = -9; offset <= 9; offset += 3) {
                    Novice::DrawLine(ax + offset, ay, wx + offset / 2, wy, 0x853D42FFu);
                }
                Novice::DrawLine(ax, ay, wx, wy, 0xE68D72FFu);
                Novice::DrawEllipse(wx, wy, 11, 11, 0.0f, 0xF4B09AFFu, kFillModeSolid);
                Novice::DrawEllipse(ax, ay, 24, 9, 0.0f, 0x9A4848FFu, kFillModeSolid);
            }
            if (wire.phase == WIRE_BOUND) {
                // 捕まった後はカウンター不可。ワイヤーの接触前にJを押す。
                int cx = (int)player.x + 18 + shakeX;
                int cy = (int)player.y + 24 + shakeY;
                int r = 38;
                Novice::DrawEllipse(cx, cy, r, r, 0.0f,
                    0xFF6060FFu, kFillModeWireFrame);
                Novice::ScreenPrintf(470, 135, "BOUND! COUNTER BEFORE CONTACT NEXT TIME");
                Novice::ScreenPrintf(500, 160, "TAIL TIMER: %d", wire.timer);
            }
            if (wireEscapeEffect > 0) {
                int cx = (int)player.x + 18 + shakeX;
                int cy = (int)player.y + 24 + shakeY;
                int elapsed = 30 - wireEscapeEffect;
                int radius = 20 + elapsed * 9;
                Novice::DrawEllipse(cx, cy, radius, radius, 0.0f,
                    WithAlpha(0x9CEFFFFFu, wireEscapeEffect * 8), kFillModeWireFrame);
                Novice::ScreenPrintf(530, 200, "TAIL JUST COUNTER!");
            }
            // 突進予告中と落雷前は着弾地点に赤い予告リングを表示する。
            if (rushLightningReady && (charge.phase == CHARGE_WARNING ||
                charge.phase == CHARGE_LEAVE || charge.phase == CHARGE_RUSH)) {
                for (int i = 0; i < kRushLightningCount; ++i) {
                    const bool pending = charge.phase != CHARGE_RUSH ||
                        charge.timer < 1 + i * kRushLightningInterval;
                    if (pending) {
                        int cx = (int)lightningX[i] + shakeX;
                        int cy = (int)kGroundY + shakeY;
                        int alpha = 110 + (int)(65.0f * sinf((float)charge.timer * 0.2f));
                        Novice::DrawEllipse(cx, cy, 48, 12, 0.0f,
                            WithAlpha(0xFF555500u, alpha), kFillModeWireFrame);
                    }
                }
            }
            // 2枚の画像を通常合成＋加算合成で重ね、3本の雷を描画する。
            for (int i = 0; i < kRushLightningCount; ++i) {
                DrawImageLightning(lightningTexture1, lightningTexture2,
                    lightningX[i] + shakeX, kGroundY + shakeY, lightningTimer[i]);
            }

            // 龍のブレス：予告時は口元に炎を集め、発射中はうねる炎・火の粉・着弾炎を描画。
            // 見た目だけを変更。DragonBreathの長さ、方向、当たり判定には触れない。
            DrawDragonFireBreath(breath, breathCircleTexture, shakeX, shakeY);
            DrawDragonFireBreath(breathSecond, breathCircleTexture, shakeX, shakeY);
            // maru.pngを加算合成で重ねて、炎に光の層を追加する。
            DrawBreathGlow(breath, maruTexture, shakeX, shakeY);
            DrawBreathGlow(breathSecond, maruTexture, shakeX, shakeY);
            if (breath.hitFlash > 0 || breathSecond.hitFlash > 0) {
                Novice::ScreenPrintf(475, 215, "BREATH HIT! (TEST INVINCIBLE)");
            }

            if (dashDodgeEffect > 0) {
                Novice::ScreenPrintf(520, 240, "DASH DODGE!");
                Novice::DrawEllipse((int)player.x + 18 + shakeX,
                    (int)player.y + 24 + shakeY, 40, 48, 0.0f,
                    WithAlpha(0x78EFFFFFu, dashDodgeEffect * 11), kFillModeWireFrame);
            }

            // 反射弾の残像を弾本体より先に描く（青い光の軌跡）。
            // 新しい残像ほど大きく明るく、古い残像ほど小さく透明にする。
            for (int i = 0; i < kMaxBullets; ++i) {
                const Bullet& b = bullets[i];
                if (!b.active || !b.reflected) continue;
                for (int j = b.trailCount - 1; j >= 0; --j) {
                    float fade = (float)(kTrailLength - j) / (float)kTrailLength;
                    int radius = 2 + (int)((b.dashReflected ? 8.0f : 5.0f) * fade);
                    int alpha = (int)((b.dashReflected ? 235.0f : 160.0f) * fade);
                    int x = (int)b.trail[j].x + shakeX;
                    int y = (int)b.trail[j].y + shakeY;
                    // 外側の淡い光と中心の明るい光を重ねる
                    Novice::DrawEllipse(x, y, radius + 3, radius + 3, 0.0f,
                        WithAlpha(b.dashReflected ? 0xB2F5FF00u : 0x379BFF00u, alpha / 3), kFillModeSolid);
                    Novice::DrawEllipse(x, y, radius, radius, 0.0f,
                        WithAlpha(b.dashReflected ? 0xE5FFFFFFu : 0x86E9FF00u, alpha), kFillModeSolid);
                }
            }
            // 巨大魔力球のチャージ中は、龍の口元に青白い光が集まる。
            if (giantChargeTimer > 0) {
                float t = (float)giantChargeTimer / (float)kGiantChargeFrames;
                float cx = boss.x + kDragonMouthOffsetX + shakeX;
                float cy = boss.y + kDragonMouthY + shakeY;
                int r = (int)(16.0f + 52.0f * t);
                Novice::SetBlendMode(kBlendModeAdd);
                // チャージ中も maru.png の光が徐々に大きくなる。
                DrawGiantOrbGlow(cx, cy, (float)r, maruTexture,
                    (float)giantChargeTimer, false);
                if (giantOrbTexture >= 0) {
                    const float size = (float)r * 2.0f;
                    Novice::DrawSprite((int)(cx - r), (int)(cy - r), giantOrbTexture,
                        size / 256.0f, size / 256.0f, 0.0f,
                        WithAlpha(0x76DFFFFFu, 205));
                }
                else {
                    Novice::DrawEllipse((int)cx, (int)cy, r, r, 0.0f,
                        WithAlpha(0x76DFFFFFu, 185), kFillModeSolid);
                }
                Novice::SetBlendMode(kBlendModeNormal);
                Novice::ScreenPrintf(850, 120, "GIANT MAGIC CHARGE!");
            }
            // ボスの弾（巨大魔力球は青い加算発光）
            for (int i = 0; i < kMaxBullets; ++i) {
                if (bullets[i].active && bullets[i].giant) {
                    const Bullet& b = bullets[i];
                    int x = (int)b.x + shakeX, y = (int)b.y + shakeY;
                    unsigned int outerColor = b.reflectLevel == 3 ? 0xB2F5FF00u : 0x247DFF00u;
                    // 出現時は小さく、24Fで一回転しながら元のサイズまで大きくなる。
                    // 位置・当たり判定・追尾速度は変更しない（見た目だけの演出）。
                    float appearT = (float)b.appearanceTimer / (float)kGiantAppearFrames;
                    if (appearT > 1.0f) appearT = 1.0f;
                    const float easedT = appearT * appearT * (3.0f - 2.0f * appearT);
                    const float appearScale = 0.15f + 0.85f * easedT;
                    const float spinAngle = (1.0f - easedT) * -6.2831853f +
                        (float)giantGlowFrame * 0.018f; // 登場後もゆっくり回転
                    const float visibleRadius = kGiantRadius * appearScale;
                    Novice::SetBlendMode(kBlendModeAdd);
                    // ブレス用 maru.png の青白い発光も本体の拡大に合わせる。
                    DrawGiantOrbGlow((float)x, (float)y, visibleRadius,
                        maruTexture, (float)giantGlowFrame + (float)i * 3.0f,
                        b.reflectLevel >= 2);
                    Novice::DrawEllipse(x, y, (int)(90.0f * appearScale),
                        (int)(90.0f * appearScale), 0.0f,
                        WithAlpha(outerColor, 35), kFillModeSolid);
                    // kougeki1.png を中心基準で回転・拡大描画する。
                    if (giantOrbTexture >= 0) {
                        const float drawSize = visibleRadius * 2.0f;
                        const unsigned int orbColor = b.reflectLevel == 3 ? 0xE5FFFFFFu :
                            (b.reflectLevel == 2 ? 0xB2F5FFFFu : 0x6FDFFFFFu);
                        // DrawSpriteの回転基準による位置ずれを防ぐため、
                        // 魔法陣の4頂点を弾の中心座標を基準に回転させて描画する。
                        const float half = drawSize * 0.5f;
                        const float cs = cosf(spinAngle);
                        const float sn = sinf(spinAngle);
                        const float centerX = b.x + (float)shakeX;
                        const float centerY = b.y + (float)shakeY;
                        const float localX[4] = { -half, half, -half, half };
                        const float localY[4] = { -half, -half, half, half };
                        int px[4], py[4];
                        for (int v = 0; v < 4; ++v) {
                            px[v] = (int)roundf(centerX + localX[v] * cs - localY[v] * sn);
                            py[v] = (int)roundf(centerY + localX[v] * sn + localY[v] * cs);
                        }
                        Novice::DrawQuad(px[0], py[0], px[1], py[1],
                            px[2], py[2], px[3], py[3],
                            0, 0, 256, 256, giantOrbTexture, orbColor);
                    }
                    else {
                        // 読み込みに失敗した場合も攻撃が見えるよう円で代替する。
                        Novice::DrawEllipse(x, y, (int)(65.0f * appearScale),
                            (int)(65.0f * appearScale), 0.0f,
                            WithAlpha(0x2EAFFF00u, 125), kFillModeSolid);
                        Novice::DrawEllipse(x, y, (int)(46.0f * appearScale),
                            (int)(46.0f * appearScale), 0.0f,
                            WithAlpha(0x87E5FF00u, 185), kFillModeSolid);
                    }
                    Novice::SetBlendMode(kBlendModeNormal);
                }
                else if (bullets[i].active) {
                    Novice::DrawEllipse((int)bullets[i].x + shakeX, (int)bullets[i].y + shakeY,
                        bullets[i].dashReflected ? 11 : 8, bullets[i].dashReflected ? 11 : 8, 0.0f,
                        bullets[i].dashReflected ? 0xE5FFFFFFu : (bullets[i].reflected ? 0x70DFFFFFu : 0xFFD16AFFu), kFillModeSolid);
                }
            }
            // ボスと弾より手前にカウンターの光を描画
            DrawCounterEffect(counterEffect, shakeX, shakeY);
            // 強化反射弾がボスに当たった際の青白い爆発
            if (specialImpactTimer > 0) {
                int elapsed = kDashCounterImpactFrames - specialImpactTimer;
                int cx = (int)specialImpactX + shakeX;
                int cy = (int)specialImpactY + shakeY;
                int alpha = 255 * specialImpactTimer / kDashCounterImpactFrames;
                for (int i = 0; i < 3; ++i) {
                    int radius = 15 + elapsed * 9 - i * 20;
                    if (radius > 0) Novice::DrawEllipse(cx, cy, radius, radius, 0.0f,
                        WithAlpha(0xA8EFFFFFu, alpha - i * 30), kFillModeWireFrame);
                }
                if (elapsed < 6) {
                    int len = 100 - elapsed * 12;
                    Novice::DrawLine(cx - len, cy, cx + len, cy, WithAlpha(0xFFFFFFFFu, alpha));
                    Novice::DrawLine(cx, cy - len, cx, cy + len, WithAlpha(0xFFFFFFFFu, alpha));
                }
            }
            // 最前面のキャラクターレイヤー：ボス・しっぽ・ブレス・弾より後に描画する。
            // プレイヤー（青い矩形）
            Novice::DrawBox((int)player.x + shakeX, (int)player.y + shakeY, 36, 48,
                0.0f, 0x66BFFFFF, kFillModeSolid);
            // プレイヤーの前側に剣を描画（左右反転・ダッシュ追従・画面シェイク対応）
            DrawSword(player, swordSwingTimer, shakeX, shakeY);
            // 通常攻撃の判定をデバッグ表示（当たり判定と見た目の調整用）
            if (normalSwordActive && swordSwingTimer <= kSwordHitStart &&
                swordSwingTimer >= kSwordHitEnd) {
                float hitX = player.direction == 1
                    ? player.x + 18.0f : player.x + 18.0f - kSwordHitWidth;
                float hitY = player.y + 24.0f - kSwordHitHeight * 0.5f;
                Novice::DrawBox((int)hitX + shakeX, (int)hitY + shakeY,
                    (int)kSwordHitWidth, (int)kSwordHitHeight, 0.0f,
                    0x66EFFFFFu, kFillModeWireFrame);
            }
            // 全画面フラッシュは最前面。徐々に透明にして視界を戻す。
            if (specialFlashTimer > 0) {
                int alpha = 165 * specialFlashTimer / kDashCounterFlashFrames;
                Novice::DrawBox(0, 0, kWidth, kHeight, 0.0f,
                    WithAlpha(0xE8FFFFFFu, alpha), kFillModeSolid);
            }
            Novice::ScreenPrintf(30, 30, "A/D : MOVE  SPACE : JUMP  SHIFT : DASH  H : SWORD  J : COUNTER");
            Novice::ScreenPrintf(30, 55, "J PRESS : 8F COUNTER WINDOW");
            if (justWindow) Novice::ScreenPrintf(30, 105, "JUST COUNTER WINDOW");
            Novice::ScreenPrintf(30, 155, "AIR DASH: HOLD W (UP) / S (DOWN) + SHIFT");
            Novice::ScreenPrintf(30, 180, "WIRE: PRESS J WHEN THE RING TURNS BLUE!");
            Novice::ScreenPrintf(30, 205, "DASH: INVINCIBLE VS BREATH / WIRE / BULLETS");
            if (dashCooldown > 0) Novice::ScreenPrintf(30, 130, "DASH COOLDOWN: %d", dashCooldown);
            else Novice::ScreenPrintf(30, 130, "DASH READY");
            if (counterEffect.timer > 0 && counterEffect.enhanced)
                Novice::ScreenPrintf(475, 100, "DASH JUST COUNTER!");
            // カウンター文字は画面演出・プレイヤーより前面に描画する。
            DrawCounterText(counterText, parryTexture, perfectTexture, breakTexture);
            // ボスHPゲージ：画面上部に固定し、画面シェイクの影響を受けない。
            const int hpBarX = 560;
            const int hpBarY = 22;
            const int hpBarWidth = 800;
            const int hpBarHeight = 24;
            const float hpRatio = (float)boss.hp / (float)kBossMaxHP;
            Novice::DrawBox(hpBarX - 3, hpBarY - 3, hpBarWidth + 6,
                hpBarHeight + 6, 0.0f, 0xB9D8E8FFu, kFillModeSolid);
            Novice::DrawBox(hpBarX, hpBarY, hpBarWidth, hpBarHeight,
                0.0f, 0x292331FFu, kFillModeSolid);
            if (boss.hp > 0) {
                Novice::DrawBox(hpBarX, hpBarY, (int)(hpBarWidth * hpRatio),
                    hpBarHeight, 0.0f, 0xD83C52FFu, kFillModeSolid);
            }
            Novice::ScreenPrintf(hpBarX + 330, hpBarY + 4,
                "HP %d / %d", boss.hp, kBossMaxHP);

            if (hitStopTimer > 0) Novice::ScreenPrintf(30, 80, "HIT STOP");
            else if (slowTimer > 0) Novice::ScreenPrintf(30, 80, "SLOW MOTION 25%%");
            break;
        }
        case SCENE_GAMEOVER:
            Novice::ScreenPrintf(860, 510, "GAME OVER");
            break;
        case SCENE_CLEAR:
            Novice::ScreenPrintf(860, 510, "GAME CLEAR");
            break;
        case SCENE_CREDITS:
            Novice::ScreenPrintf(910, 400, "CREDITS");
            // ここに制作者名・素材提供元
            Novice::ScreenPrintf(810, 530, "Created by: (your name)");
            Novice::ScreenPrintf(800, 650, "Press ENTER to return to TITLE");
            break;
        }
        if (currentScene == SCENE_GAMEOVER || currentScene == SCENE_CLEAR) {
            Novice::ScreenPrintf(810, 590, "Press ENTER to change scene");
        }

        // ===== コントローラー入力デバッグ（全シーン共通） =====
        // 全シーンの描画後に表示し、背景に隠れないようにする。
        Novice::ScreenPrintf(20, 20, "DEBUG TEST");
        Novice::ScreenPrintf(20, 50, "Controller: %d", Controller::IsConnected() ? 1 : 0);
        Novice::ScreenPrintf(20, 80, "A Down: %d", Controller::IsDown(kPadButton10) ? 1 : 0);
        Novice::ScreenPrintf(20, 110, "A Pressed: %d", Controller::IsPressed(kPadButton10) ? 1 : 0);
        Novice::ScreenPrintf(20, 140, "Scene: %d", (int)currentScene);
        Novice::ScreenPrintf(20, 170, "Enter Key: %d", keys[DIK_RETURN] != 0);

        /// ↑描画処理ここまで
        ///

        // フレームの終了
        Novice::EndFrame();

        // ESCキーが押されたらループを抜ける
        if (preKeys[DIK_ESCAPE] == 0 && keys[DIK_ESCAPE] != 0) {
            break;
        }
    }

    // ライブラリの終了
    Novice::Finalize();
    return 0;
}
