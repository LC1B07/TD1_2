#include <Novice.h>
#include <Windows.h>
#include <cstring>
#include <cmath>

#include "game.h"
#include "player.h"
#include "boss.h"
#include "effect.h"
#include "scene.h"
#include "fullScreen.h"

const char kWindowTitle[] = "TD1_2";

// Windowsアプリのエントリーポイント
int WINAPI WinMain(
    _In_ HINSTANCE,
    _In_opt_ HINSTANCE,
    _In_ LPSTR,
    _In_ int
) {

    // ライブラリの初期化
    Novice::Initialize(kWindowTitle, kWidth, kHeight);

	// ウィンドウハンドルとフルスクリーン切り替え用の変数
    HWND gameWindow = GetActiveWindow();
    RECT windowRect = {};
    LONG_PTR windowStyle = 0;
    bool isFullScreen = false;

    if (gameWindow == nullptr)
    {
        OutputDebugStringA("gameWindow is nullptr\n");
    }

    // キー入力
    char keys[256] = { 0 };
    char preKeys[256] = { 0 };

    // シーン
    Scene currentScene = SCENE_TITLE;

    int titleSelection = 0;

    // ゲームデータ
    Player player = {};
    Boss boss = {};

    Bullet bullets[kMaxBullets] = {};

    ResetBossBattle(player, boss, bullets);

    CounterEffect counterEffect = {};

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

    DashWave dashWave = {};

    DashAfterimage dashImages[kDashAfterimageCount] = {};

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

    // ゲームループ
    while (Novice::ProcessMessage() == 0) {

        Novice::BeginFrame();

        // キー入力
        memcpy(preKeys, keys, 256);
        Novice::GetHitKeyStateAll(keys);

        // F11でフルスクリーン切り替え
        if (keys[DIK_F11] != 0 && preKeys[DIK_F11] == 0)
        {
            ToggleFullScreen(
                gameWindow,
                windowRect,
                windowStyle,
                isFullScreen
            );
        }

        bool justWindow = false;

        // --------------------------------
        // 更新処理
        // --------------------------------

        bool enter =
            preKeys[DIK_RETURN] == 0 &&
            keys[DIK_RETURN] != 0;

        bool up =
            preKeys[DIK_W] == 0 &&
            keys[DIK_W] != 0;

        bool down =
            preKeys[DIK_S] == 0 &&
            keys[DIK_S] != 0;

        // タイトル画面
        if (currentScene == SCENE_TITLE) {

            if (up || down) {
                titleSelection = 1 - titleSelection;
            }

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

                    for (int i = 0; i < kDashAfterimageCount; ++i) {
                        dashImages[i] = {};
                    }

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
                } else {

                    currentScene = SCENE_CREDITS;
                }
            }
        }

        // ボス戦
        else if (currentScene == SCENE_BOSS) {

            // 演出タイマー
            if (dashCooldown > 0) --dashCooldown;

            if (dashWave.timer > 0) --dashWave.timer;

            for (int i = 0; i < kDashAfterimageCount; ++i) {
                if (dashImages[i].timer > 0) {
                    --dashImages[i].timer;
                }
            }

            if (counterCooldown > 0) --counterCooldown;
            if (counterEffect.timer > 0) --counterEffect.timer;
            if (bossFlash > 0) --bossFlash;
            if (swordHitFlashTimer > 0) --swordHitFlashTimer;
            if (shakeTimer > 0) --shakeTimer;
            if (specialFlashTimer > 0) --specialFlashTimer;
            if (specialImpactTimer > 0) --specialImpactTimer;

            // Jキー
            const bool justPressedJ =
                keys[DIK_J] && !preKeys[DIK_J];

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

            if (hitStopTimer == 0 && counterWindowTimer > 0) {

                --counterWindowTimer;

                if (counterWindowTimer == 0) {
                    counterArmed = false;
                }
            }

            // ゲーム速度
            float gameSpeed = 1.0f;

            if (hitStopTimer > 0) {

                gameSpeed = 0.0f;
                --hitStopTimer;
            } else if (slowTimer > 0) {

                gameSpeed = kCounterSlowScale;
                --slowTimer;
            }

            // ダッシュ入力
            if (keys[DIK_LSHIFT] && !preKeys[DIK_LSHIFT]) {
                dashRequest = true;
            }

            if (!keys[DIK_LSHIFT]) {
                dashRequest = false;
            }

            // ジャンプ入力
            if (keys[DIK_SPACE] && !preKeys[DIK_SPACE]) {
                jumpRequest = true;
            }

            // Hキー通常攻撃
            if (keys[DIK_H] && !preKeys[DIK_H]) {

                swordRequest = true;
                normalSwordRequest = true;
            }

            // ゲームフレーム計算
            gameFrameAccumulator += gameSpeed;

            bool updateGame = false;

            if (gameFrameAccumulator >= 1.0f) {

                gameFrameAccumulator -= 1.0f;
                updateGame = true;
            }

            if (updateGame) {

                // 剣
                if (swordSwingTimer > 0) {
                    --swordSwingTimer;
                }

                if (swordSwingTimer == 0) {
                    normalSwordActive = false;
                }

                if (swordRequest) {

                    swordSwingTimer = kSwordSwingFrames;

                    normalSwordActive = normalSwordRequest;

                    swordHitBoss = false;
                }

                swordRequest = false;
                normalSwordRequest = false;

                // プレイヤー移動
                float moveX = 0.0f;

                if (keys[DIK_A]) moveX -= 1.0f;
                if (keys[DIK_D]) moveX += 1.0f;

                if (moveX < 0.0f) {
                    player.direction = -1;
                } else if (moveX > 0.0f) {
                    player.direction = 1;
                }

                // ジャンプ
                if (jumpRequest && player.onGround) {

                    player.velocityY = kJumpPower;
                    player.onGround = false;
                }

                jumpRequest = false;

                // ダッシュ
                if (dashRequest && dashCooldown == 0 && dashTimer == 0) {

                    float inputDashX = moveX;
                    float inputDashY = 0.0f;

                    if (!player.onGround) {

                        if (keys[DIK_W]) inputDashY -= 1.0f;
                        if (keys[DIK_S]) inputDashY += 1.0f;
                    }

                    if (inputDashX == 0.0f && inputDashY == 0.0f) {

                        inputDashX = (float)player.direction;
                    }

                    float dashLength =
                        sqrtf(inputDashX * inputDashX +
                            inputDashY * inputDashY);

                    dashDirX = inputDashX / dashLength;
                    dashDirY = inputDashY / dashLength;

                    dashTimer = kDashDuration;
                    dashCooldown = kDashCooldown;

                    dashWave = {
                        player.x + 18.0f,
                        player.y + 24.0f,
                        kDashWaveFrames
                    };

                    if (!player.onGround) {
                        player.velocityY = 0.0f;
                    }
                }

                dashRequest = false;

                // ダッシュ移動
                if (dashTimer > 0) {

                    player.x += dashDirX * kDashSpeed;
                    player.y += dashDirY * kDashSpeed;

                    --dashTimer;

                    player.velocityY = 0.0f;

                    dashImages[dashImageIndex] = {
                        player.x,
                        player.y,
                        14
                    };

                    dashImageIndex =
                        (dashImageIndex + 1) % kDashAfterimageCount;
                } else {

                    player.x += moveX * kMoveSpeed;

                    player.velocityY += kGravity;

                    player.y += player.velocityY;
                }

                // 着地
                if (player.y >= kGroundY - 48.0f) {

                    player.y = kGroundY - 48.0f;

                    player.velocityY = 0.0f;

                    player.onGround = true;

                    if (dashDirY > 0.0f) {
                        dashTimer = 0;
                    }
                } else {

                    player.onGround = false;
                }

                // 画面外制限
                if (player.x < 0.0f) {
                    player.x = 0.0f;
                }

                if (player.x > kWidth - 36.0f) {
                    player.x = kWidth - 36.0f;
                }

                if (player.y < 0.0f) {

                    player.y = 0.0f;

                    if (player.velocityY < 0.0f) {
                        player.velocityY = 0.0f;
                    }
                }

                // 剣の攻撃判定
                const bool swordHitActive =
                    normalSwordActive &&
                    swordSwingTimer <= kSwordHitStart &&
                    swordSwingTimer >= kSwordHitEnd;

                if (swordHitActive && !swordHitBoss) {

                    float hitX = player.direction == 1
                        ? player.x + 18.0f
                        : player.x + 18.0f - kSwordHitWidth;

                    float hitY =
                        player.y + 24.0f - kSwordHitHeight * 0.5f;

                    if (IsHit(
                        hitX, hitY,
                        kSwordHitWidth, kSwordHitHeight,
                        boss.x, boss.y,
                        100.0f, 120.0f
                    )) {

                        swordHitBoss = true;

                        swordHitFlashTimer = 12;
                        bossFlash = 12;
                    }
                }

                // ボスの弾発射
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

                            if (length < 0.001f) {
                                length = 1.0f;
                            }

                            bullets[i] = {
                                startX,
                                startY,
                                dx / length * 7.0f,
                                dy / length * 7.0f,
                                true,
                                false
                            };

                            break;
                        }
                    }
                }

                // 弾の移動と当たり判定
                for (int i = 0; i < kMaxBullets; ++i) {

                    Bullet& b = bullets[i];

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

                    b.x += b.vx;
                    b.y += b.vy;

                    // カウンター判定
                    if (!b.reflected && justWindow) {

                        float dx = b.x - (player.x + 18.0f);
                        float dy = b.y - (player.y + 24.0f);

                        if (dx * dx + dy * dy <= 90.0f * 90.0f) {

                            const bool dashJustCounter = dashTimer > 0;

                            counterEffect = {
                                b.x,
                                b.y,
                                30,
                                dashJustCounter
                            };

                            shakeTotalFrames = dashJustCounter
                                ? kDashCounterShakeFrames
                                : kCounterShakeFrames;

                            shakeMaxPower = dashJustCounter
                                ? kDashCounterShakePower
                                : kCounterShakePower;

                            shakeTimer = shakeTotalFrames;

                            hitStopTimer = dashJustCounter
                                ? kDashCounterHitStopFrames
                                : kCounterHitStopFrames;

                            slowTimer = dashJustCounter
                                ? kDashCounterSlowFrames
                                : kCounterSlowFrames;

                            if (dashJustCounter) {
                                specialFlashTimer = kDashCounterFlashFrames;
                            }

                            gameFrameAccumulator = 0.0f;

                            counterCooldown = kCounterRetryCooldown;

                            float toBossX = boss.x + 50.0f - b.x;
                            float toBossY = boss.y + 60.0f - b.y;

                            float length =
                                sqrtf(toBossX * toBossX + toBossY * toBossY);

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

                            counterWindowTimer = 0;

                            swordSwingTimer = kSwordSwingFrames;

                            normalSwordActive = false;
                            counterArmed = false;
                        }
                    }

                    // 画面外
                    if (b.x < -20 || b.x > kWidth + 20 ||
                        b.y < -20 || b.y > kHeight + 20) {

                        b.active = false;
                        continue;
                    }

                    // ボスへの命中
                    if (b.reflected &&
                        IsHit(
                            boss.x, boss.y, 100, 120,
                            b.x - 8, b.y - 8, 16, 16
                        )) {

                        b.active = false;

                        bossFlash = 12;

                        if (b.dashReflected) {

                            specialImpactX = b.x;
                            specialImpactY = b.y;

                            specialImpactTimer = kDashCounterImpactFrames;
                        }

                        continue;
                    }

                    // プレイヤーへの命中
                    if (!b.reflected &&
                        IsHit(
                            player.x, player.y, 36, 48,
                            b.x - 8, b.y - 8, 16, 16
                        )) {

                        b.active = false;
                        continue;
                    }
                }
            }

            // テスト用クリア
            if (enter && currentScene == SCENE_BOSS) {
                currentScene = SCENE_CLEAR;
            }
        }

        // その他のシーン
        else if (enter) {

            switch (currentScene) {

            case SCENE_GAMEOVER:
                currentScene = SCENE_TITLE;
                titleSelection = 0;
                break;

            case SCENE_CLEAR:
                currentScene = SCENE_TITLE;
                titleSelection = 0;
                break;

            case SCENE_CREDITS:
                currentScene = SCENE_TITLE;
                titleSelection = 1;
                break;

            default:
                break;
            }
        }

        // --------------------------------
        // 描画処理
        // --------------------------------

        shakeX = 0;
        shakeY = 0;

        if (currentScene == SCENE_BOSS && shakeTimer > 0) {

            float t = (float)shakeTimer / (float)shakeTotalFrames;

            float power = shakeMaxPower * t;

            int step = shakeTotalFrames - shakeTimer;

            shakeX = (int)(sinf(step * 2.4f + 0.7f) * power);

            shakeY = (int)(cosf(step * 3.1f + 0.4f) * power);
        }

        DrawBackground();

        switch (currentScene) {

            // タイトル
        case SCENE_TITLE:

            Novice::ScreenPrintf(608, 190, "TITLE");

            DrawMenuButton(
                480, 290,
                "GAME START",
                titleSelection == 0
            );

            DrawMenuButton(
                480, 380,
                "CREDITS",
                titleSelection == 1
            );

            Novice::ScreenPrintf(
                490, 500,
                "W / S : SELECT    ENTER : OK"
            );

            break;

            // ボス戦
        case SCENE_BOSS:

            Novice::ScreenPrintf(550, 60, "BOSS BATTLE");

            // 地面
            Novice::DrawBox(
                0,
                (int)kGroundY + shakeY,
                kWidth,
                4,
                0.0f,
                0x53627CFF,
                kFillModeSolid
            );

            // ダッシュ残像
            for (int i = 0; i < kDashAfterimageCount; ++i) {

                const DashAfterimage& a = dashImages[i];

                if (a.timer <= 0) {
                    continue;
                }

                int alpha = a.timer * 9;

                Novice::DrawBox(
                    (int)a.x + shakeX,
                    (int)a.y + shakeY,
                    36,
                    48,
                    0.0f,
                    WithAlpha(0x4BBFFF00u, alpha),
                    kFillModeSolid
                );
            }

            // ダッシュ衝撃波
            if (dashWave.timer > 0) {

                int elapsed = kDashWaveFrames - dashWave.timer;

                int cx = (int)dashWave.x + shakeX;
                int cy = (int)dashWave.y + shakeY;

                int alpha =
                    (dashWave.timer * 190) / kDashWaveFrames;

                int radius = 12 + elapsed * 7;

                Novice::DrawEllipse(
                    cx, cy,
                    radius, radius,
                    0.0f,
                    WithAlpha(0x7DDFFF00u, alpha),
                    kFillModeWireFrame
                );

                if (radius > 18) {

                    Novice::DrawEllipse(
                        cx, cy,
                        radius - 18,
                        radius - 18,
                        0.0f,
                        WithAlpha(0xC5F7FF00u, alpha / 2),
                        kFillModeWireFrame
                    );
                }

                if (elapsed < 4) {

                    Novice::DrawEllipse(
                        cx, cy,
                        22 + elapsed * 5,
                        22 + elapsed * 5,
                        0.0f,
                        WithAlpha(0xFFFFFFFFu, 140 - elapsed * 35),
                        kFillModeWireFrame
                    );
                }
            }

            // プレイヤー
            Novice::DrawBox(
                (int)player.x + shakeX,
                (int)player.y + shakeY,
                36,
                48,
                0.0f,
                0x66BFFFFF,
                kFillModeSolid
            );

            DrawSword(
                player,
                swordSwingTimer,
                shakeX,
                shakeY
            );

            // 剣の攻撃判定表示
            if (normalSwordActive &&
                swordSwingTimer <= kSwordHitStart &&
                swordSwingTimer >= kSwordHitEnd) {

                float hitX = player.direction == 1
                    ? player.x + 18.0f
                    : player.x + 18.0f - kSwordHitWidth;

                float hitY =
                    player.y + 24.0f - kSwordHitHeight * 0.5f;

                Novice::DrawBox(
                    (int)hitX + shakeX,
                    (int)hitY + shakeY,
                    (int)kSwordHitWidth,
                    (int)kSwordHitHeight,
                    0.0f,
                    0x66EFFFFFu,
                    kFillModeWireFrame
                );
            }

            // ボス
            Novice::DrawBox(
                (int)boss.x + shakeX,
                (int)boss.y + shakeY,
                100,
                120,
                0.0f,
                bossFlash > 0
                ? 0xFFFFFFFFu
                : 0xE15B65FF,
                kFillModeSolid
            );

            if (swordHitFlashTimer > 0) {
                Novice::ScreenPrintf(560, 190, "SWORD HIT!");
            }

            // 反射弾の残像
            for (int i = 0; i < kMaxBullets; ++i) {

                const Bullet& b = bullets[i];

                if (!b.active || !b.reflected) {
                    continue;
                }

                for (int j = b.trailCount - 1; j >= 0; --j) {

                    float fade =
                        (float)(kTrailLength - j) /
                        (float)kTrailLength;

                    int radius = 2 +
                        (int)((b.dashReflected ? 8.0f : 5.0f) * fade);

                    int alpha =
                        (int)((b.dashReflected ? 235.0f : 160.0f) * fade);

                    int x = (int)b.trail[j].x + shakeX;
                    int y = (int)b.trail[j].y + shakeY;

                    Novice::DrawEllipse(
                        x, y,
                        radius + 3,
                        radius + 3,
                        0.0f,
                        WithAlpha(
                            b.dashReflected
                            ? 0xB2F5FF00u
                            : 0x379BFF00u,
                            alpha / 3
                        ),
                        kFillModeSolid
                    );

                    Novice::DrawEllipse(
                        x, y,
                        radius,
                        radius,
                        0.0f,
                        WithAlpha(
                            b.dashReflected
                            ? 0xE5FFFFFFu
                            : 0x86E9FF00u,
                            alpha
                        ),
                        kFillModeSolid
                    );
                }
            }

            // 弾
            for (int i = 0; i < kMaxBullets; ++i) {

                if (bullets[i].active) {

                    Novice::DrawEllipse(
                        (int)bullets[i].x + shakeX,
                        (int)bullets[i].y + shakeY,
                        bullets[i].dashReflected ? 11 : 8,
                        bullets[i].dashReflected ? 11 : 8,
                        0.0f,
                        bullets[i].dashReflected
                        ? 0xE5FFFFFFu
                        : (bullets[i].reflected
                            ? 0x70DFFFFFu
                            : 0xFFD16AFFu),
                        kFillModeSolid
                    );
                }
            }

            // カウンターエフェクト
            DrawCounterEffect(
                counterEffect,
                shakeX,
                shakeY
            );

            // 強化反射弾の爆発
            if (specialImpactTimer > 0) {

                int elapsed =
                    kDashCounterImpactFrames - specialImpactTimer;

                int cx = (int)specialImpactX + shakeX;
                int cy = (int)specialImpactY + shakeY;

                int alpha =
                    255 * specialImpactTimer / kDashCounterImpactFrames;

                for (int i = 0; i < 3; ++i) {

                    int radius = 15 + elapsed * 9 - i * 20;

                    if (radius > 0) {

                        Novice::DrawEllipse(
                            cx, cy,
                            radius, radius,
                            0.0f,
                            WithAlpha(0xA8EFFFFFu, alpha - i * 30),
                            kFillModeWireFrame
                        );
                    }
                }

                if (elapsed < 6) {

                    int len = 100 - elapsed * 12;

                    Novice::DrawLine(
                        cx - len, cy,
                        cx + len, cy,
                        WithAlpha(0xFFFFFFFFu, alpha)
                    );

                    Novice::DrawLine(
                        cx, cy - len,
                        cx, cy + len,
                        WithAlpha(0xFFFFFFFFu, alpha)
                    );
                }
            }

            // 画面フラッシュ
            if (specialFlashTimer > 0) {

                int alpha =
                    165 * specialFlashTimer / kDashCounterFlashFrames;

                Novice::DrawBox(
                    0, 0,
                    kWidth, kHeight,
                    0.0f,
                    WithAlpha(0xE8FFFFFFu, alpha),
                    kFillModeSolid
                );
            }

            // 操作説明
            Novice::ScreenPrintf(
                30, 30,
                "A/D : MOVE  SPACE : JUMP  SHIFT : DASH  H : SWORD  J : COUNTER"
            );

            Novice::ScreenPrintf(
                30, 55,
                "J PRESS : 8F COUNTER WINDOW  |  ENTER : TEST CLEAR"
            );

            if (justWindow) {
                Novice::ScreenPrintf(30, 105, "JUST COUNTER WINDOW");
            }

            Novice::ScreenPrintf(
                30, 155,
                "AIR DASH: HOLD W (UP) / S (DOWN) + SHIFT"
            );

            if (dashCooldown > 0) {

                Novice::ScreenPrintf(
                    30, 130,
                    "DASH COOLDOWN: %d",
                    dashCooldown
                );
            } else {

                Novice::ScreenPrintf(30, 130, "DASH READY");
            }

            if (counterEffect.timer > 0 && counterEffect.enhanced) {

                Novice::ScreenPrintf(
                    475, 100,
                    "DASH JUST COUNTER!"
                );
            }

            if (hitStopTimer > 0) {

                Novice::ScreenPrintf(30, 80, "HIT STOP");
            } else if (slowTimer > 0) {

                Novice::ScreenPrintf(30, 80, "SLOW MOTION 25%%");
            }

            break;

            // ゲームオーバー
        case SCENE_GAMEOVER:

            Novice::ScreenPrintf(540, 330, "GAME OVER");

            break;

            // クリア
        case SCENE_CLEAR:

            Novice::ScreenPrintf(540, 330, "GAME CLEAR");

            break;

            // クレジット
        case SCENE_CREDITS:

            Novice::ScreenPrintf(590, 220, "CREDITS");

            Novice::ScreenPrintf(
                490, 350,
                "Created by: (your name)"
            );

            Novice::ScreenPrintf(
                480, 470,
                "Press ENTER to return to TITLE"
            );

            break;
        }

        if (currentScene == SCENE_GAMEOVER ||
            currentScene == SCENE_CLEAR) {

            Novice::ScreenPrintf(
                490, 390,
                "Press ENTER to change scene"
            );
        }

        Novice::EndFrame();

        // ESCキー
        if (preKeys[DIK_ESCAPE] == 0 &&
            keys[DIK_ESCAPE] != 0) {

            break;
        }
    }

    // 終了処理
    Novice::Finalize();

    return 0;
}