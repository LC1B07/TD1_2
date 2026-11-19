#include "bossB.h"

// BossBは別担当者向けのひな形。竜の行動・攻撃は未実装。
namespace BossB {
void Reset(State& boss) { boss = {}; }

void Update(State& boss, Scene& currentScene, const char keys[], const char preKeys[], bool leftMousePressed, bool rightMousePressed) {
	(void)leftMousePressed;
	(void)rightMousePressed;
	++boss.frames;
	if (keys[DIK_RETURN] && !preKeys[DIK_RETURN]) {
		currentScene = SCENE_TITLE;
	}
}

void Draw(const State& boss) {
	(void)boss;
	Novice::ScreenPrintf(60, 80, "BOSS B - DRAGON (WORK IN PROGRESS)");
	Novice::ScreenPrintf(60, 110, "Press ENTER to return to title");
}
} // namespace BossB
