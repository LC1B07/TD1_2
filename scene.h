
#pragma once

#include "game.h"

// タイトル更新
void UpdateTitleScene(
    GameState& state,
    Scene& currentScene,
    int& titleSelection,
    const char keys[],
    const char preKeys[]
);

// ボス戦更新
void UpdateBossScene(
    GameState& state,
    Scene& currentScene,
    const char keys[],
    const char preKeys[]
);

// その他のシーン更新
void UpdateOtherScene(
    Scene& currentScene,
    int& titleSelection,
    const char keys[],
    const char preKeys[]
);

// タイトル描画
void DrawTitleScene(int titleSelection);

// ボス戦描画
void DrawBossScene(GameState& state);

// ゲームオーバー描画
void DrawGameOverScene();

// クリア描画
void DrawClearScene();

// クレジット描画
void DrawCreditsScene();

// メニューボタン描画
void DrawMenuButton(
    int x,
    int y,
    const char* label,
    bool selected
);
