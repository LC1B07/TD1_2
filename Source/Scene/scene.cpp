
#include "../Scene/scene.h"

#include "../Player/player.h"
#include "../Boss/boss.h"
#include "../Bullet/bullet.h"
#include "../Effect/effect.h"

// メニューボタン描画
void DrawMenuButton(
    int x,
    int y,
    const char* label,
    bool selected
) {

    unsigned int fill =
        selected ? 0x293D59FF : 0x1D2536FF;

    unsigned int edge =
        selected ? 0x8BCBFFFF : 0x53627CFF;

    // 影
    Novice::DrawBox(
        x + 5, y + 7,
        320, 64,
        0.0f,
        WithAlpha(0x00000000u, 100),
        kFillModeSolid
    );

    // ボタン本体
    Novice::DrawBox(
        x, y,
        320, 64,
        0.0f,
        fill,
        kFillModeSolid
    );

    // 選択時のハイライト
    if (selected) {

        Novice::DrawBox(
            x + 3, y + 3,
            314, 9,
            0.0f,
            WithAlpha(0x8BCBFF00u, 75),
            kFillModeSolid
        );
    }

    // 枠線
    Novice::DrawBox(
        x, y,
        320, 64,
        0.0f,
        edge,
        kFillModeWireFrame
    );

    // 文字
    Novice::ScreenPrintf(
        x + 95, y + 23,
        "%s",
        label
    );

    if (selected) {
        Novice::ScreenPrintf(x + 25, y + 23, ">");
    }
}

// ボス戦状態の初期化
static void ResetGameState(GameState& state) {

    state = {};

    ResetBossBattle(
        state.player,
        state.boss,
        state.bullets
    );

    state.shakeTotalFrames = kCounterShakeFrames;
    state.shakeMaxPower = kCounterShakePower;
}

// タイトル更新
void UpdateTitleScene(
    GameState& state,
    Scene& currentScene,
    int& titleSelection,
    const char keys[],
    const char preKeys[]
) {

    bool up =
        preKeys[DIK_W] == 0 &&
        keys[DIK_W] != 0;

    bool down =
        preKeys[DIK_S] == 0 &&
        keys[DIK_S] != 0;

    bool enter =
        preKeys[DIK_RETURN] == 0 &&
        keys[DIK_RETURN] != 0;

    if (up || down) {
        titleSelection = 1 - titleSelection;
    }

    if (enter) {

        if (titleSelection == 0) {

            ResetGameState(state);

            currentScene = SCENE_BOSS;
        } else {

            currentScene = SCENE_CREDITS;
        }
    }
}

// ボス戦更新
void UpdateBossScene(
    GameState& state,
    Scene& currentScene,
    const char keys[],
    const char preKeys[]
) {

    // 演出タイマー
    UpdateEffectTimers(state);

    // Jキー
    const bool justPressedJ =
        keys[DIK_J] && !preKeys[DIK_J];

    if (justPressedJ) {

        state.swordRequest = true;
        state.normalSwordRequest = false;

        if (state.counterCooldown == 0) {

            state.counterArmed = true;

            state.counterWindowTimer = kJustCounterWindow;

            state.counterCooldown = kCounterRetryCooldown;
        }
    }

    state.justWindow =
        state.counterArmed &&
        state.counterWindowTimer > 0;

    if (
        state.hitStopTimer == 0 &&
        state.counterWindowTimer > 0
        ) {

        --state.counterWindowTimer;

        if (state.counterWindowTimer == 0) {
            state.counterArmed = false;
        }
    }

    // ゲーム速度
    float gameSpeed = 1.0f;

    if (state.hitStopTimer > 0) {

        gameSpeed = 0.0f;

        --state.hitStopTimer;
    } else if (state.slowTimer > 0) {

        gameSpeed = kCounterSlowScale;

        --state.slowTimer;
    }

    // ダッシュ入力
    if (keys[DIK_LSHIFT] && !preKeys[DIK_LSHIFT]) {
        state.dashRequest = true;
    }

    if (!keys[DIK_LSHIFT]) {
        state.dashRequest = false;
    }

    // ジャンプ入力
    if (keys[DIK_SPACE] && !preKeys[DIK_SPACE]) {
        state.jumpRequest = true;
    }

    // Hキー通常攻撃
    if (keys[DIK_H] && !preKeys[DIK_H]) {

        state.swordRequest = true;
        state.normalSwordRequest = true;
    }

    // ゲームフレーム計算
    state.gameFrameAccumulator += gameSpeed;

    bool updateGame = false;

    if (state.gameFrameAccumulator >= 1.0f) {

        state.gameFrameAccumulator -= 1.0f;

        updateGame = true;
    }

    if (updateGame) {

        // プレイヤー更新
        UpdatePlayer(state, keys);

        // ボス更新
        UpdateBoss(
            state.boss,
            state.player,
            state.bullets
        );

        // 弾更新
        UpdateBullets(state);
    }

    // テスト用クリア
    if (
        keys[DIK_RETURN] &&
        !preKeys[DIK_RETURN]
        ) {
        currentScene = SCENE_CLEAR;
    }
}

// その他のシーン更新
void UpdateOtherScene(
    Scene& currentScene,
    int& titleSelection,
    const char keys[],
    const char preKeys[]
) {

    bool enter =
        preKeys[DIK_RETURN] == 0 &&
        keys[DIK_RETURN] != 0;

    if (!enter) {
        return;
    }

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

// タイトル描画
void DrawTitleScene(int titleSelection) {

    Novice::ScreenPrintf(608, 190, "TITLE");

    DrawMenuButton(
        480, 290,
        "ゲームを始める",
        titleSelection == 0
    );

    DrawMenuButton(
        480, 380,
        "CREDITS",
        titleSelection == 1
    );

    Novice::ScreenPrintf(
        490, 500,
        "W / S : 選択    ENTER : 決定"
    );
}

// ボス戦描画
void DrawBossScene(GameState& state) {

    const int shakeX = state.shakeX;
    const int shakeY = state.shakeY;

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

        const DashAfterimage& a = state.dashImages[i];

        if (a.timer <= 0) {
            continue;
        }

        Novice::DrawBox(
            (int)a.x + shakeX,
            (int)a.y + shakeY,
            36,
            48,
            0.0f,
            WithAlpha(0x4BBFFF00u, a.timer * 9),
            kFillModeSolid
        );
    }

    // ダッシュ衝撃波
    if (state.dashWave.timer > 0) {

        int elapsed = kDashWaveFrames - state.dashWave.timer;

        int cx = (int)state.dashWave.x + shakeX;
        int cy = (int)state.dashWave.y + shakeY;

        int alpha =
            state.dashWave.timer * 190 / kDashWaveFrames;

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
        (int)state.player.x + shakeX,
        (int)state.player.y + shakeY,
        36,
        48,
        0.0f,
        0x66BFFFFF,
        kFillModeSolid
    );

    // 剣
    DrawSword(
        state.player,
        state.swordSwingTimer,
        shakeX,
        shakeY
    );

    // 剣の攻撃判定表示
    if (
        state.normalSwordActive &&
        state.swordSwingTimer <= kSwordHitStart &&
        state.swordSwingTimer >= kSwordHitEnd
        ) {

        float hitX = state.player.direction == 1
            ? state.player.x + 18.0f
            : state.player.x + 18.0f - kSwordHitWidth;

        float hitY =
            state.player.y + 24.0f - kSwordHitHeight * 0.5f;

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
        (int)state.boss.x + shakeX,
        (int)state.boss.y + shakeY,
        100,
        120,
        0.0f,
        state.bossFlash > 0
        ? 0xFFFFFFFFu
        : 0xE15B65FF,
        kFillModeSolid
    );

    if (state.swordHitFlashTimer > 0) {
        Novice::ScreenPrintf(560, 190, "SWORD HIT!");
    }

    // 反射弾の残像
    for (int i = 0; i < kMaxBullets; ++i) {

        const Bullet& b = state.bullets[i];

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

        const Bullet& b = state.bullets[i];

        if (!b.active) {
            continue;
        }

        Novice::DrawEllipse(
            (int)b.x + shakeX,
            (int)b.y + shakeY,
            b.dashReflected ? 11 : 8,
            b.dashReflected ? 11 : 8,
            0.0f,
            b.dashReflected
            ? 0xE5FFFFFFu
            : (b.reflected
                ? 0x70DFFFFFu
                : 0xFFD16AFFu),
            kFillModeSolid
        );
    }

    // カウンターエフェクト
    DrawCounterEffect(
        state.counterEffect,
        shakeX,
        shakeY
    );

    // 強化反射弾の爆発
    if (state.specialImpactTimer > 0) {

        int elapsed =
            kDashCounterImpactFrames - state.specialImpactTimer;

        int cx = (int)state.specialImpactX + shakeX;
        int cy = (int)state.specialImpactY + shakeY;

        int alpha =
            255 * state.specialImpactTimer / kDashCounterImpactFrames;

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
    if (state.specialFlashTimer > 0) {

        int alpha =
            165 * state.specialFlashTimer / kDashCounterFlashFrames;

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

    if (state.justWindow) {
        Novice::ScreenPrintf(30, 105, "JUST COUNTER WINDOW");
    }

    Novice::ScreenPrintf(
        30, 155,
        "AIR DASH: HOLD W (UP) / S (DOWN) + SHIFT"
    );

    if (state.dashCooldown > 0) {

        Novice::ScreenPrintf(
            30, 130,
            "DASH COOLDOWN: %d",
            state.dashCooldown
        );
    } else {

        Novice::ScreenPrintf(30, 130, "DASH READY");
    }

    if (
        state.counterEffect.timer > 0 &&
        state.counterEffect.enhanced
        ) {

        Novice::ScreenPrintf(
            475, 100,
            "DASH JUST COUNTER!"
        );
    }

    if (state.hitStopTimer > 0) {
        Novice::ScreenPrintf(30, 80, "HIT STOP");
    } else if (state.slowTimer > 0) {
        Novice::ScreenPrintf(30, 80, "SLOW MOTION 25%%");
    }
}

// ゲームオーバー描画
void DrawGameOverScene() {
    Novice::ScreenPrintf(540, 330, "GAME OVER");
    Novice::ScreenPrintf(490, 390, "Press ENTER to change scene");
}

// クリア描画
void DrawClearScene() {
    Novice::ScreenPrintf(540, 330, "GAME CLEAR");
    Novice::ScreenPrintf(490, 390, "Press ENTER to change scene");
}

// クレジット描画
void DrawCreditsScene() {

    Novice::ScreenPrintf(590, 220, "CREDITS");

    Novice::ScreenPrintf(
        490, 350,
        "Created by: (your name)"
    );

    Novice::ScreenPrintf(
        480, 470,
        "ENTER を 押して タイトル に 戻る"
    );
}
