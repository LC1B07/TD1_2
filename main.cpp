#include <Novice.h>
#include <cstring>
#include <cmath>

// シーンの種類
enum Scene {
    SCENE_TITLE,
    SCENE_BOSS,
    SCENE_GAMEOVER,
    SCENE_CLEAR,
    SCENE_CREDITS // クレジット画面を追加
};

// 全シーン共通の背景色（パーティクルなし）
const int kWidth = 1280;
const int kHeight = 720;
void DrawBackground() {
    Novice::DrawBox(0, 0, kWidth, kHeight, 0.0f, 0x151826FF, kFillModeSolid);
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
const float kGroundY = 620.0f;
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
const int kShotInterval = 180;
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

struct Boss {
    float x, y;
    int shotTimer;
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
    player = { 140.0f, kGroundY - 48.0f, 1, 0.0f, true };
    boss = { 1020.0f, 300.0f, 0 };
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

const char kWindowTitle[] = "LC1D_03_イシダ_ノリヒコ_タイトル";

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

    // ライブラリの初期化
    Novice::Initialize(kWindowTitle, 1280, 720);

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

    // ウィンドウの×ボタンが押されるまでループ
    while (Novice::ProcessMessage() == 0) {
        // フレームの開始
        Novice::BeginFrame();

        // キー入力を受け取る
        memcpy(preKeys, keys, 256);
        Novice::GetHitKeyStateAll(keys);

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
                    currentScene = SCENE_BOSS;
                }
                else currentScene = SCENE_CREDITS;
            }
        }
        else if (currentScene == SCENE_BOSS) {
            // 演出はヒットストップ・スロー中も実時間で進める
            if (dashCooldown > 0) --dashCooldown;
            if (dashWave.timer > 0) --dashWave.timer;
            for (int i = 0; i < kDashAfterimageCount; ++i) {
                if (dashImages[i].timer > 0) --dashImages[i].timer;
            }
            if (counterCooldown > 0) --counterCooldown;
            if (counterEffect.timer > 0) --counterEffect.timer;
            if (bossFlash > 0) --bossFlash;
            if (swordHitFlashTimer > 0) --swordHitFlashTimer;
            if (shakeTimer > 0) --shakeTimer;
            if (specialFlashTimer > 0) --specialFlashTimer;
            if (specialImpactTimer > 0) --specialImpactTimer;
            // Jを押した瞬間に剣を振り、クールダウン中でなければジャスト判定を開始する。
            // 長押しガードは廃止。押しっぱなしでも判定は再発生しない。
            const bool justPressedJ = keys[DIK_J] && !preKeys[DIK_J];
            if (justPressedJ) {
                swordRequest = true;
                normalSwordRequest = false;
                if (counterCooldown == 0) {
                    counterArmed = true;
                    counterWindowTimer = kJustCounterWindow;
                    counterCooldown = kCounterRetryCooldown;
                }
            }
            justWindow = counterArmed && counterWindowTimer > 0;
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
            if (keys[DIK_SPACE] && !preKeys[DIK_SPACE]) jumpRequest = true;
            // Hを押した瞬間に剣を振る。ヒットストップ中の入力も予約する。
            if (keys[DIK_H] && !preKeys[DIK_H]) {
                swordRequest = true;
                normalSwordRequest = true;
            }
            gameFrameAccumulator += gameSpeed;
            bool updateGame = false;
            if (gameFrameAccumulator >= 1.0f) {
                gameFrameAccumulator -= 1.0f;
                updateGame = true;
            }

            if (updateGame) {
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
                    if (IsHit(hitX, hitY, kSwordHitWidth, kSwordHitHeight,
                        boss.x, boss.y, 100.0f, 120.0f)) {
                        swordHitBoss = true;
                        swordHitFlashTimer = 12;
                        bossFlash = 12; // 試作用：命中するとボスが白く光る
                    }
                }

                // ボスが約3秒に1回、プレイヤーの方向に弾を撃つ
                ++boss.shotTimer;
                if (boss.shotTimer >= kShotInterval) {
                    boss.shotTimer = 0;
                    for (int i = 0; i < kMaxBullets; ++i) {
                        if (!bullets[i].active) {
                            float startX = boss.x;
                            float startY = boss.y + 48.0f;
                            float dx = player.x + 18.0f - startX;
                            float dy = player.y + 24.0f - startY;
                            float length = sqrtf(dx * dx + dy * dy);
                            if (length < 0.001f) length = 1.0f;
                            bullets[i] = { startX, startY,
                                dx / length * 7.0f, dy / length * 7.0f, true, false };
                            break;
                        }
                    }
                }
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
                    b.x += b.vx;
                    b.y += b.vy;
                    // ジャスト判定中はプレイヤー中心から90px以内に入った弾を反射。
                    // 成功した瞬間に、既存の強いシェイク・停止・スローを開始する。
                    if (!b.reflected && justWindow) {
                        float dx = b.x - (player.x + 18.0f);
                        float dy = b.y - (player.y + 24.0f);
                        if (dx * dx + dy * dy <= 90.0f * 90.0f) {
                            // Shiftキーではなく「実際にダッシュしているか」で判定する。
                            const bool dashJustCounter = dashTimer > 0;
                            counterEffect = { b.x, b.y, 30, dashJustCounter };
                            shakeTotalFrames = dashJustCounter ? kDashCounterShakeFrames : kCounterShakeFrames;
                            shakeMaxPower = dashJustCounter ? kDashCounterShakePower : kCounterShakePower;
                            shakeTimer = shakeTotalFrames;
                            hitStopTimer = dashJustCounter ? kDashCounterHitStopFrames : kCounterHitStopFrames;
                            slowTimer = dashJustCounter ? kDashCounterSlowFrames : kCounterSlowFrames;
                            if (dashJustCounter) specialFlashTimer = kDashCounterFlashFrames;
                            gameFrameAccumulator = 0.0f;
                            counterCooldown = kCounterRetryCooldown;
                            float toBossX = boss.x + 50.0f - b.x;
                            float toBossY = boss.y + 60.0f - b.y;
                            float length = sqrtf(toBossX * toBossX + toBossY * toBossY);
                            if (length < 0.001f) length = 1.0f;
                            const float reflectedSpeed = dashJustCounter ? kDashCounterBulletSpeed : kCounterBulletSpeed;
                            b.vx = toBossX / length * reflectedSpeed;
                            b.vy = toBossY / length * reflectedSpeed;
                            b.reflected = true;
                            b.dashReflected = dashJustCounter;
                            // 反射前の黄色い弾の軌跡は引き継がない
                            b.trailCount = 0;
                            // 1回の押下で複数の弾を反射しない
                            counterWindowTimer = 0;
                            swordSwingTimer = kSwordSwingFrames; // 成功時も剣を振る動きを強調
                            normalSwordActive = false; // Jのカウンターは通常攻撃扱いにしない
                            counterArmed = false;
                        }
                    }
                    if (b.x < -20 || b.x > kWidth + 20 ||
                        b.y < -20 || b.y > kHeight + 20) {
                        b.active = false;
                        continue;
                    }
                    if (b.reflected && IsHit(boss.x, boss.y, 100, 120,
                        b.x - 8, b.y - 8, 16, 16)) {
                        b.active = false;
                        bossFlash = 12; // ボスに命中したら白く点滅
                        if (b.dashReflected) {
                            specialImpactX = b.x;
                            specialImpactY = b.y;
                            specialImpactTimer = kDashCounterImpactFrames;
                        }
                        continue;
                    }
                    if (!b.reflected && IsHit(player.x, player.y, 36, 48, b.x - 8, b.y - 8, 16, 16)) {
                        b.active = false;
                        // テスト中はガードしていなくても死亡しない
                        // 弾だけ消してボス戦を続行する
                        continue;
                    }
                }
            }
            // 動作確認用：Enterでクリア画面へ
            if (enter && currentScene == SCENE_BOSS) currentScene = SCENE_CLEAR;
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

        // 全シーン共通の単色背景を描画する
        DrawBackground();

        // 現在のシーンに応じた画面を描画する
        switch (currentScene) {
        case SCENE_TITLE:
            Novice::ScreenPrintf(608, 190, "TITLE");
            DrawMenuButton(480, 290, "GAME START", titleSelection == 0);
            DrawMenuButton(480, 380, "CREDITS", titleSelection == 1);
            Novice::ScreenPrintf(490, 500, "W / S : SELECT    ENTER : OK");
            break;
        case SCENE_BOSS:
            Novice::ScreenPrintf(550, 60, "BOSS BATTLE");
            // ジャンプ用の地面を描画する。
            Novice::DrawBox(0, (int)kGroundY + shakeY, kWidth, 4, 0.0f, 0x53627CFF, kFillModeSolid);
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
            // ボス（赤い矩形）
            Novice::DrawBox((int)boss.x + shakeX, (int)boss.y + shakeY, 100, 120,
                0.0f, bossFlash > 0 ? 0xFFFFFFFFu : 0xE15B65FF, kFillModeSolid);
            if (swordHitFlashTimer > 0)
                Novice::ScreenPrintf(560, 190, "SWORD HIT!");
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
            // ボスの弾（黄色い円）
            for (int i = 0; i < kMaxBullets; ++i) {
                if (bullets[i].active) {
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
            // 全画面フラッシュは最前面。徐々に透明にして視界を戻す。
            if (specialFlashTimer > 0) {
                int alpha = 165 * specialFlashTimer / kDashCounterFlashFrames;
                Novice::DrawBox(0, 0, kWidth, kHeight, 0.0f,
                    WithAlpha(0xE8FFFFFFu, alpha), kFillModeSolid);
            }
            Novice::ScreenPrintf(30, 30, "A/D : MOVE  SPACE : JUMP  SHIFT : DASH  H : SWORD  J : COUNTER");
            Novice::ScreenPrintf(30, 55, "J PRESS : 8F COUNTER WINDOW  |  ENTER : TEST CLEAR");
            if (justWindow) Novice::ScreenPrintf(30, 105, "JUST COUNTER WINDOW");
            Novice::ScreenPrintf(30, 155, "AIR DASH: HOLD W (UP) / S (DOWN) + SHIFT");
            if (dashCooldown > 0) Novice::ScreenPrintf(30, 130, "DASH COOLDOWN: %d", dashCooldown);
            else Novice::ScreenPrintf(30, 130, "DASH READY");
            if (counterEffect.timer > 0 && counterEffect.enhanced)
                Novice::ScreenPrintf(475, 100, "DASH JUST COUNTER!");
            if (hitStopTimer > 0) Novice::ScreenPrintf(30, 80, "HIT STOP");
            else if (slowTimer > 0) Novice::ScreenPrintf(30, 80, "SLOW MOTION 25%%");
            break;
        case SCENE_GAMEOVER:
            Novice::ScreenPrintf(540, 330, "GAME OVER");
            break;
        case SCENE_CLEAR:
            Novice::ScreenPrintf(540, 330, "GAME CLEAR");
            break;
        case SCENE_CREDITS:
            Novice::ScreenPrintf(590, 220, "CREDITS");
            // ここに制作者名・素材提供元
            Novice::ScreenPrintf(490, 350, "Created by: (your name)");
            Novice::ScreenPrintf(480, 470, "Press ENTER to return to TITLE");
            break;
        }
        if (currentScene == SCENE_GAMEOVER || currentScene == SCENE_CLEAR) {
            Novice::ScreenPrintf(490, 390, "Press ENTER to change scene");
        }

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
