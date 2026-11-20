
#pragma once

#include "../Game/game.h"

// プレイヤー更新
void UpdatePlayer(GameState& state, const char keys[]);

// 剣の共通攻撃判定（命中後の処理は各ボス側）
bool IsSwordHitTarget(const GameState& state, float targetX, float targetY, float targetWidth, float targetHeight);

// 剣の描画
void DrawSword(const Player& player, int swingTimer, int shakeX, int shakeY);
