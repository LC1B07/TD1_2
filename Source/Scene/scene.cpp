
#include "../Scene/scene.h"

#include "../Bullet/bullet.h"
#include "../Controller/Controller.h"
#include "../Effect/effect.h"
#include "../Player/player.h"

// メニューボタン描画
void DrawMenuButton(int x, int y, const char* label, bool selected) {

	// ボタンのサイズ
	const int buttonWidth = 350;
	const int buttonHeight = 80;

	unsigned int fill = selected ? 0x293D59FF : 0x1D2536FF;

	unsigned int edge = selected ? 0x8BCBFFFF : 0x53627CFF;

	// 影
	Novice::DrawBox(x + 5, y + 7, buttonWidth, buttonHeight, 0.0f, WithAlpha(0x00000000u, 100), kFillModeSolid);

	// ボタン本体
	Novice::DrawBox(x, y, buttonWidth, buttonHeight, 0.0f, fill, kFillModeSolid);

	// 選択時のハイライト
	if (selected) {

		Novice::DrawBox(x + 3, y + 3, buttonWidth - 6, 10, 0.0f, WithAlpha(0x8BCBFF00u, 75), kFillModeSolid);
	}

	// 枠線
	Novice::DrawBox(x, y, buttonWidth, buttonHeight, 0.0f, edge, kFillModeWireFrame);

	// 文字
	Novice::ScreenPrintf(x + 95, y + 32, "%s", label);

	// 選択マーク
	if (selected) {
		Novice::ScreenPrintf(x + 25, y + 32, ">");
	}
}

// タイトル更新
void UpdateTitleScene(GameState& state, Scene& currentScene, int& titleSelection, const char keys[], const char preKeys[]) {
	(void)state; // BossAの初期化はシーンに入る際にmain.cppで行う。

	// 左キー（A）
	bool left = false;

	if (preKeys[DIK_A] == 0 && keys[DIK_A] != 0 || Controller::IsPressed(Controller::Button::kPadButton2)) {
		left = true;
	}

	// 右キー（D）
	bool right = false;

	if (preKeys[DIK_D] == 0 && keys[DIK_D] != 0 || Controller::IsPressed(Controller::Button::kPadButton3)) {
		right = true;
	}

	bool enter = false;

	if (preKeys[DIK_RETURN] == 0 && keys[DIK_RETURN] != 0 || Controller::IsPressed(Controller::Button::kPadButton10)) {
		enter = true;
	}

	// 左に移動
	if (left) {

		titleSelection--;

		// 一番左から左に移動したら一番右へ
		if (titleSelection < 0) {
			titleSelection = 3;
		}
	}

	// 右に移動
	if (right) {

		titleSelection++;

		// 一番右から右に移動したら一番左へ
		if (titleSelection > 3) {
			titleSelection = 0;
		}
	}

	if (enter) {

		switch (titleSelection) {

		case 0:
			currentScene = SCENE_TUTORIAL;
			break;

		case 1:
			currentScene = SCENE_BOSS_SELECT;
			break;

		case 2:
			currentScene = SCENE_BGM;
			break;

		case 3:
			currentScene = SCENE_CREDITS;
			break;
		}
	}
}

// その他のシーン更新
void UpdateOtherScene(const char keys[], const char preKeys[]) {

	bool enter = false;

	if (preKeys[DIK_RETURN] == 0 && keys[DIK_RETURN] != 0) {
		enter = true;
	}

	if (enter == false) {
		return;
	}
}

// タイトル描画
void DrawTitleScene(int titleSelection) {

	Novice::ScreenPrintf(960, 540, "TITLE");

	DrawMenuButton(100, 780, "TUTORIAL", titleSelection == 0);

	DrawMenuButton(530, 780, "PLAY", titleSelection == 1);

	DrawMenuButton(960, 780, "BGM", titleSelection == 2);

	DrawMenuButton(1460, 780, "CREDIT", titleSelection == 3);

	Novice::ScreenPrintf(380, 980, "A / D : SELECT    ENTER : GO");
}

// ボス選択画面。A/Dまたはパッドで鬼/竜を選択。
void UpdateBossSelectScene(Scene& scene, int& selection, const char keys[], const char preKeys[]) {
	const bool left = (keys[DIK_A] && !preKeys[DIK_A]) || Controller::IsPressed(Controller::Button::kPadButton2);
	const bool right = (keys[DIK_D] && !preKeys[DIK_D]) || Controller::IsPressed(Controller::Button::kPadButton3);
	if (left || right)
		selection = 1 - selection;
	const bool enter = (keys[DIK_RETURN] && !preKeys[DIK_RETURN]) || Controller::IsPressed(Controller::Button::kPadButton10);
	if (enter)
		scene = selection == 0 ? SCENE_BOSS_A : SCENE_BOSS_B;
	if (keys[DIK_BACK] && !preKeys[DIK_BACK])
		scene = SCENE_TITLE;
}

void DrawBossSelectScene(int selection) {
	Novice::ScreenPrintf(830, 240, "SELECT BOSS");
	DrawMenuButton(480, 480, "BOSS A - ONI", selection == 0);
	DrawMenuButton(1060, 480, "BOSS B - DRAGON", selection == 1);
	Novice::ScreenPrintf(720, 680, "A / D : SELECT  ENTER : DECIDE  BACKSPACE : TITLE");
}

void DrawBgmScene() {
	Novice::ScreenPrintf(860, 300, "BGM SETTINGS (WORK IN PROGRESS)");
	Novice::ScreenPrintf(780, 380, "Press ENTER to return to TITLE");
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

	Novice::ScreenPrintf(490, 350, "Created by: (your name)");

	Novice::ScreenPrintf(480, 470, "ENTER を 押して タイトル に 戻る");
}
