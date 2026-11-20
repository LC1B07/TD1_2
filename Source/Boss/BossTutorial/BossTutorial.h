#pragma once
#include "../../Game/game.h"

// チュートリアル担当: 実際のボスの挙動は、このフォルダで追加。
namespace BossTutorial {
struct State {
	int frames = 0; // 仮データ
};
void Reset(State& boss);
void Update(State& boss, Scene& currentScene, const char keys[], const char preKeys[], bool leftMousePressed, bool rightMousePressed);
void Draw(const State& boss);
} // namespace BossTutorial
