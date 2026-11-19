#include <Novice.h>
#include <Windows.h>
#include <cmath>
#include <cstring>

#include "Source/Boss/BossA/BossA.h"
#include "Source/Boss/BossB/BossB.h"
#include "Source/Boss/BossTutorial/BossTutorial.h"
#include "Source/Controller/Controller.h"
#include "Source/FullScreen/fullScreen.h"
#include "Source/Game/game.h"
#include "Source/Scene/scene.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

const char kWindowTitle[] = "TD1_2";

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
	Novice::Initialize(kWindowTitle, kWidth, kHeight);
	Controller::Initialize();

	HWND gameWindow = GetActiveWindow();
	RECT windowRect = {};
	LONG_PTR windowStyle = 0;
	bool isFullScreen = false;
	char keys[256] = {};
	char preKeys[256] = {};
	bool previousLeftDown = false;
	bool previousRightDown = false;

	Scene currentScene = SCENE_TITLE;
	Scene lastScene = SCENE_TITLE;
	int titleSelection = 0;
	int bossSelection = 0;
	GameState oniState = {};
	BossB::State dragonState = {};
	BossTutorial::State tutorialState = {};
	BossA::Reset(oniState);
	BossB::Reset(dragonState);
	BossTutorial::Reset(tutorialState);

	while (Novice::ProcessMessage() == 0) {
		Novice::BeginFrame();
		memcpy(preKeys, keys, sizeof(keys));
		Novice::GetHitKeyStateAll(keys);
		// Friend's controller input is updated in every scene, not just the title.
		Controller::Update();

		const bool focused = GetForegroundWindow() == gameWindow;
		const bool leftDown = focused && ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
		const bool rightDown = focused && ((GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0);
		const bool leftPressed = leftDown && !previousLeftDown;
		const bool rightPressed = rightDown && !previousRightDown;
		previousLeftDown = leftDown;
		previousRightDown = rightDown;

		if (keys[DIK_F11] && !preKeys[DIK_F11]) {
			ToggleFullScreen(gameWindow, windowRect, windowStyle, isFullScreen);
		}

#ifdef USE_IMGUI
		ImGui::SetNextWindowSize(ImVec2(400.0f, 200.0f), ImGuiCond_FirstUseEver);
		ImGui::Begin("Debug");
		ImGui::Combo(
		    "currentScene", reinterpret_cast<int*>(&currentScene),
		    "SCENE_TITLE\0SCENE_BGM\0SCENE_CREDITS\0SCENE_TUTORIAL\0"
		    "SCENE_BOSS_SELECT\0SCENE_BOSS_A\0SCENE_BOSS_B\0SCENE_CLEAR\0SCENE_GAMEOVER\0");
		ImGui::End();
#endif

		// Reset only when ENTERING a scene. Selecting the same scene does not reset the battle.
		if (currentScene != lastScene) {
			switch (currentScene) {
			case SCENE_BOSS_A:
				BossA::Reset(oniState);
				break;
			case SCENE_BOSS_B:
				BossB::Reset(dragonState);
				break;
			case SCENE_TUTORIAL:
				BossTutorial::Reset(tutorialState);
				break;
			case SCENE_BOSS_SELECT:
				bossSelection = 0;
				break;
			default:
				break;
			}
			lastScene = currentScene;
		}

		DrawBackground();
		switch (currentScene) {
		case SCENE_TITLE:
			UpdateTitleScene(oniState, currentScene, titleSelection, keys, preKeys);
			DrawTitleScene(titleSelection);
			break;

		case SCENE_BGM:
			DrawBgmScene();
			if ((keys[DIK_RETURN] && !preKeys[DIK_RETURN]) || Controller::IsPressed(Controller::Button::kPadButton10))
				currentScene = SCENE_TITLE;
			break;

		case SCENE_CREDITS:
			DrawCreditsScene();
			if ((keys[DIK_RETURN] && !preKeys[DIK_RETURN]) || Controller::IsPressed(Controller::Button::kPadButton10))
				currentScene = SCENE_TITLE;
			break;

		case SCENE_TUTORIAL:
			BossTutorial::Update(tutorialState, currentScene, keys, preKeys, leftPressed, rightPressed);
			if (currentScene == SCENE_TUTORIAL)
				BossTutorial::Draw(tutorialState);
			break;

		case SCENE_BOSS_SELECT:
			UpdateBossSelectScene(currentScene, bossSelection, keys, preKeys);
			if (currentScene == SCENE_BOSS_SELECT)
				DrawBossSelectScene(bossSelection);
			break;

		case SCENE_BOSS_A:
			BossA::Update(oniState, currentScene, keys, preKeys, leftPressed, rightPressed);
			if (currentScene == SCENE_BOSS_A) {
				// Compute shake BEFORE drawing, so the current frame's shake is visible.
				oniState.shakeX = 0;
				oniState.shakeY = 0;
				if (oniState.shakeTimer > 0 && oniState.shakeTotalFrames > 0) {
					const float progress = (float)oniState.shakeTimer / (float)oniState.shakeTotalFrames;
					const float power = oniState.shakeMaxPower * progress;
					const int step = oniState.shakeTotalFrames - oniState.shakeTimer;
					oniState.shakeX = (int)(sinf(step * 2.4f + 0.7f) * power);
					oniState.shakeY = (int)(cosf(step * 3.1f + 0.4f) * power);
				}
				BossA::Draw(oniState);
			}
			break;

		case SCENE_BOSS_B:
			BossB::Update(dragonState, currentScene, keys, preKeys, leftPressed, rightPressed);
			if (currentScene == SCENE_BOSS_B)
				BossB::Draw(dragonState);
			break;

		case SCENE_CLEAR:
			DrawClearScene();
			if ((keys[DIK_RETURN] && !preKeys[DIK_RETURN]) || Controller::IsPressed(Controller::Button::kPadButton10))
				currentScene = SCENE_TITLE;
			break;

		case SCENE_GAMEOVER:
			DrawGameOverScene();
			if ((keys[DIK_RETURN] && !preKeys[DIK_RETURN]) || Controller::IsPressed(Controller::Button::kPadButton10))
				currentScene = SCENE_TITLE;
			break;
		default:
			break;
		}

		Novice::EndFrame();
		if (keys[DIK_ESCAPE] && !preKeys[DIK_ESCAPE])
			break;
	}

	Controller::StopVibration();
	Novice::Finalize();
	return 0;
}
