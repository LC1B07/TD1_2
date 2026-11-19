
#include <Novice.h>
#include <Windows.h>

#include "Source/Game/game.h"
#include "Source/Player/player.h"
#include "Source/Boss/boss.h"
#include "Source/Bullet/bullet.h"
#include "Source/Effect/effect.h"
#include "Source/Scene/scene.h"
#include "Source/FullScreen/fullScreen.h"
#include "Source/Controller/Controller.h"

// 開発補助用　GUI
#ifdef USE_IMGUI     // #ifdef USE_IMGUI と #endif　で囲う
#include <imgui.h>  
#endif

const char kWindowTitle[] = "TD1_2";

// Windowsアプリのエントリーポイント
int WINAPI WinMain(
    _In_ HINSTANCE,
    _In_opt_ HINSTANCE,
    _In_ LPSTR,
    _In_ int
) {

    // 初期化
    Novice::Initialize(kWindowTitle, kWidth, kHeight);

    // ウィンドウ情報
    HWND gameWindow = GetActiveWindow();

    RECT windowRect = {};
    LONG_PTR windowStyle = 0;

    bool isFullScreen = false;

    // キー入力
    char keys[256] = {};
    char preKeys[256] = {};

    // シーン
    Scene currentScene = SCENE_TITLE;

    int titleSelection = 0;

    // ゲーム状態
    GameState state = {};

    ResetBossBattle(
        state.player,
        state.boss,
        state.bullets
    );

    // ゲームループ
    while (Novice::ProcessMessage() == 0) {

        Novice::BeginFrame();

        // キー入力
        memcpy(preKeys, keys, 256);

        Novice::GetHitKeyStateAll(keys);

        // F11でフルスクリーン切り替え
        if (
            keys[DIK_F11] != 0 &&
            preKeys[DIK_F11] == 0
            ) {

            ToggleFullScreen(
                gameWindow,
                windowRect,
                windowStyle,
                isFullScreen
            );
        }

        // --------------------------------
        // 更新処理
        // --------------------------------

        // ウィンドウの初期サイズを設定 
        // ImGuiCond_FirstUseEver == 初回だけサイズを指定
        ImGui::SetNextWindowSize(ImVec2(400.0f, 200.0f), ImGuiCond_FirstUseEver);

        // ImGui::Begin("Debug"); と ImGui::End();　で挟むことで、ImGuiのウィンドウを作成することができる
        ImGui::Begin("Debug");
       
        ImGui::Combo(
            "currentScene",
            reinterpret_cast<int*>(&currentScene),
            "SCENE_TITLE\0SCENE_BGM\0SCENE_CREDITS\0SCENE_TUTORIAL\0SCENE_BOSS_SELECT\0SCENE_BOSS_A\0SCENE_BOSS_B\0SCENE_CLEAR\0SCENE_GAMEOVER\0"
        );

        // シーン番号を表示
        ImGui::Text("SCENE : %d", currentScene);

        // コントローラー接続状態
        ImGui::Text("Pad Connected : %d", Controller::IsConnected());

		// 矢印キーの状態
		ImGui::Text("Pad Button0 : %d", Controller::IsDown(Controller::Button::kPadButton0));  // 上ボタン
		ImGui::Text("Pad Button3 : %d", Controller::IsDown(Controller::Button::kPadButton3));  // 右ボタン
		ImGui::Text("Pad Button1 : %d", Controller::IsDown(Controller::Button::kPadButton1));  // 下ボタン
		ImGui::Text("Pad Button2 : %d", Controller::IsDown(Controller::Button::kPadButton2));  // 左ボタン

		// ボタンの状態
        ImGui::Text("Pad Button10 : %d", Controller::IsDown(Controller::Button::kPadButton10));  // Aボタン
        ImGui::Text("Pad Button11 : %d", Controller::IsDown(Controller::Button::kPadButton11));  // Bボタン
        ImGui::Text("Pad Button12 : %d", Controller::IsDown(Controller::Button::kPadButton12));  // Xボタン
        ImGui::Text("Pad Button13 : %d", Controller::IsDown(Controller::Button::kPadButton13));  // Yボタン

        // L1 ボタンの状態
        ImGui::Text("Pad Button8 : %d", Controller::IsDown(Controller::Button::kPadButton8));  // L1ボタン

        // R1 ボタンの状態
        ImGui::Text("Pad Button9 : %d", Controller::IsDown(Controller::Button::kPadButton9));  // R1ボタン

        // L2 ボタンの状態
        ImGui::Text("Pad Button14 : %d", Controller::IsDown(Controller::Button::kPadButton14));  // L2ボタン

        // L2 ボタンの状態
        ImGui::Text("Pad Button15 : %d", Controller::IsDown(Controller::Button::kPadButton15));  // L2ボタン

        // 左スティック
        ImGui::Text("Left Stick X : %.2f", Controller::GetLeftStickX());
        ImGui::Text("Left Stick Y : %.2f", Controller::GetLeftStickY());

        // 右スティック
        ImGui::Text("Right Stick X : %.2f", Controller::GetRightStickX());
        ImGui::Text("Right Stick Y : %.2f", Controller::GetRightStickY());

        ImGui::End();

        DrawBackground();

        switch (currentScene) {

        case SCENE_TITLE:

            Controller::Update();

            UpdateTitleScene(
                state,
                currentScene,
                titleSelection,
                keys,
                preKeys
            );

            DrawTitleScene(titleSelection);

            break;

		case SCENE_BGM:

            break;

		case SCENE_CREDITS:

            UpdateOtherScene(
                keys,
                preKeys
            );

            DrawCreditsScene();

            break;

        case SCENE_TUTORIAL:

            UpdateBossScene(
                state,
                keys,
                preKeys
            );

            break;

        case SCENE_BOSS_SELECT:

            break;

        case SCENE_BOSS_A:

            UpdateBossScene(
                state,
                keys,
                preKeys
            );

            DrawBossScene(state);

            break;

        case SCENE_BOSS_B:

            break;

        case SCENE_CLEAR:

            DrawClearScene();

            break;

        case SCENE_GAMEOVER:

            DrawGameOverScene();

            break;

        default:
            break;
        }

        // --------------------------------
        // 画面揺れ
        // --------------------------------

        state.shakeX = 0;
        state.shakeY = 0;

        if (
            currentScene == SCENE_BOSS_A &&
            state.shakeTimer > 0
            ) {

            float t =
                (float)state.shakeTimer /
                (float)state.shakeTotalFrames;

            float power = state.shakeMaxPower * t;

            int step =
                state.shakeTotalFrames - state.shakeTimer;

            state.shakeX =
                (int)(sinf(step * 2.4f + 0.7f) * power);

            state.shakeY =
                (int)(cosf(step * 3.1f + 0.4f) * power);
        }

        Novice::EndFrame();

        // ESCキー
        if (
            preKeys[DIK_ESCAPE] == 0 &&
            keys[DIK_ESCAPE] != 0
            ) {
            break;
        }
    }

    // 終了処理
    Novice::Finalize();

    return 0;
}
