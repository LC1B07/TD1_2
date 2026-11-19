#include <Novice.h>
#include <Windows.h>
#include <cmath>
#include <cstring>

#include "Source/Boss/BossA/bossA.h"
#include "Source/Boss/BossB/bossB.h"
#include "Source/Boss/BossTutorial/bossTutorial.h"
#include "Source/FullScreen/fullScreen.h"
#include "Source/Game/game.h"
#include "Source/Scene/scene.h"

const char kWindowTitle[] = "TD1_2 - ONI BOSS";

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

	Novice::Initialize(kWindowTitle, kWidth, kHeight);

	HWND gameWindow = GetActiveWindow();
	RECT windowRect = {};
	LONG_PTR windowStyle = 0;
	bool isFullScreen = false;

	char keys[256] = {};
	char preKeys[256] = {};

	bool previousLeftDown = false;
	bool previousRightDown = false;

	Scene currentScene = SCENE_TITLE;

	int titleSelection = 0;
	GameState state = {};

	BossA::Reset(state);
	BossB::State dragonState = {};
	BossTutorial::State tutorialState = {};
	BossB::Reset(dragonState);
	BossTutorial::Reset(tutorialState);

	while (Novice::ProcessMessage() == 0) {

		Novice::BeginFrame();
		memcpy(preKeys, keys, sizeof(keys));

		Novice::GetHitKeyStateAll(keys);

		// Windowsの左右マウス入力。クリック開始の1フレームだけtrue。
		// Noviceのウィンドウが操作対象のときに限る。
		const bool focused = (GetForegroundWindow() == gameWindow);

		const bool leftDown = focused && ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);

		const bool rightDown = focused && ((GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0);

		const bool leftPressed = leftDown && !previousLeftDown;
		const bool rightPressed = rightDown && !previousRightDown;

		previousLeftDown = leftDown;
		previousRightDown = rightDown;

		if (keys[DIK_F11] != 0 && preKeys[DIK_F11] == 0) {
			ToggleFullScreen(gameWindow, windowRect, windowStyle, isFullScreen);
		}

		// 前のフレームの揺れを消してから今回の値を計算。
		state.shakeX = 0;
		state.shakeY = 0;
		DrawBackground();

		switch (currentScene) {

		case SCENE_TITLE:

			UpdateTitleScene(state, currentScene, titleSelection, keys, preKeys);
			DrawTitleScene(titleSelection);

			break;

		case SCENE_BGM:

			break;

		case SCENE_CREDITS:

			if (keys[DIK_RETURN] != 0 && preKeys[DIK_RETURN] == 0) {
				currentScene = SCENE_TITLE;
			}

			DrawCreditsScene();

			break;

		case SCENE_TUTORIAL:

			BossTutorial::Update(tutorialState, currentScene, keys, preKeys, leftPressed, rightPressed);

			if (currentScene == SCENE_TUTORIAL) {
				BossTutorial::Draw(tutorialState);
			}

			break;

		case SCENE_BOSS_SELECT:

			break;

		case SCENE_BOSS_A:

			BossA::Update(state, currentScene, keys, preKeys, leftPressed, rightPressed);

			if (currentScene == SCENE_BOSS_A) {

				if (state.shakeTimer > 0 && state.shakeTotalFrames > 0) {

					const float t = (float)state.shakeTimer / (float)state.shakeTotalFrames;
					const float power = state.shakeMaxPower * t;
					const int step = state.shakeTotalFrames - state.shakeTimer;
					state.shakeX = (int)(sinf(step * 2.4f + 0.7f) * power);
					state.shakeY = (int)(cosf(step * 3.1f + 0.4f) * power);
				}

				BossA::Draw(state);
			}

			break;

		case SCENE_BOSS_B:

			BossB::Update(dragonState, currentScene, keys, preKeys, leftPressed, rightPressed);

			if (currentScene == SCENE_BOSS_B) {

				BossB::Draw(dragonState);
			}

			break;

		case SCENE_CLEAR:

			DrawClearScene();

			if (keys[DIK_RETURN] != 0 && preKeys[DIK_RETURN] == 0) {

				currentScene = SCENE_TITLE;
			}

			break;

		case SCENE_GAMEOVER:

			DrawGameOverScene();

			if (keys[DIK_RETURN] != 0 && preKeys[DIK_RETURN] == 0) {

				currentScene = SCENE_TITLE;
			}

			break;

		default:

			break;
		}

		Novice::EndFrame();

		if (keys[DIK_ESCAPE] != 0 && preKeys[DIK_ESCAPE] == 0) {

			break;
		}
	}
	Novice::Finalize();
	return 0;
}
