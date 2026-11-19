
#pragma once

#include "../Game/game.h"

// プレイヤー更新
void UpdatePlayer(GameState& state, const char keys[], const char preKeys[]);

// ボスごとの剣命中判定は、それぞれのボスモジュールで処理する。

// 剣の描画
void DrawSword(const Player& player, int swingTimer, int shakeX, int shakeY);
