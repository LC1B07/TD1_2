
#pragma once

#include "game.h"

// プレイヤー更新
void UpdatePlayer(
    GameState& state,
    const char keys[]
);

// 剣の攻撃判定
void CheckSwordHitBoss(GameState& state);

// 剣の描画
void DrawSword(
    const Player& player,
    int swingTimer,
    int shakeX,
    int shakeY
);
