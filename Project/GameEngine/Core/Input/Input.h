#pragma once
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <wrl.h>
#include <cstdint>
#include <Xinput.h>
#include "Utility/VectorMath.h"

using namespace Microsoft::WRL;

namespace GameEngine {
/// @brief キーボードキー列挙型
enum class KeyCode : uint8_t {
   // アルファベット
   A = DIK_A,
   B = DIK_B,
   C = DIK_C,
   D = DIK_D,
   E = DIK_E,
   F = DIK_F,
   G = DIK_G,
   H = DIK_H,
   I = DIK_I,
   J = DIK_J,
   K = DIK_K,
   L = DIK_L,
   M = DIK_M,
   N = DIK_N,
   O = DIK_O,
   P = DIK_P,
   Q = DIK_Q,
   R = DIK_R,
   S = DIK_S,
   T = DIK_T,
   U = DIK_U,
   V = DIK_V,
   W = DIK_W,
   X = DIK_X,
   Y = DIK_Y,
   Z = DIK_Z,

   // 数字
   Num0 = DIK_0,
   Num1 = DIK_1,
   Num2 = DIK_2,
   Num3 = DIK_3,
   Num4 = DIK_4,
   Num5 = DIK_5,
   Num6 = DIK_6,
   Num7 = DIK_7,
   Num8 = DIK_8,
   Num9 = DIK_9,

   // ファンクションキー
   F1 = DIK_F1,
   F2 = DIK_F2,
   F3 = DIK_F3,
   F4 = DIK_F4,
   F5 = DIK_F5,
   F6 = DIK_F6,
   F7 = DIK_F7,
   F8 = DIK_F8,
   F9 = DIK_F9,
   F10 = DIK_F10,
   F11 = DIK_F11,
   F12 = DIK_F12,

   // 特殊キー
   Escape = DIK_ESCAPE,
   Tab = DIK_TAB,
   CapsLock = DIK_CAPITAL,
   LeftShift = DIK_LSHIFT,
   RightShift = DIK_RSHIFT,
   LeftCtrl = DIK_LCONTROL,
   RightCtrl = DIK_RCONTROL,
   LeftAlt = DIK_LMENU,
   RightAlt = DIK_RMENU,
   Space = DIK_SPACE,
   Enter = DIK_RETURN,
   Backspace = DIK_BACK,
   Delete = DIK_DELETE,
   Insert = DIK_INSERT,
   Home = DIK_HOME,
   End = DIK_END,
   PageUp = DIK_PRIOR,
   PageDown = DIK_NEXT,

   // 矢印キー
   Up = DIK_UP,
   Down = DIK_DOWN,
   Left = DIK_LEFT,
   Right = DIK_RIGHT,

   // テンキー
   Numpad0 = DIK_NUMPAD0,
   Numpad1 = DIK_NUMPAD1,
   Numpad2 = DIK_NUMPAD2,
   Numpad3 = DIK_NUMPAD3,
   Numpad4 = DIK_NUMPAD4,
   Numpad5 = DIK_NUMPAD5,
   Numpad6 = DIK_NUMPAD6,
   Numpad7 = DIK_NUMPAD7,
   Numpad8 = DIK_NUMPAD8,
   Numpad9 = DIK_NUMPAD9,
   NumpadEnter = DIK_NUMPADENTER,
   NumpadPlus = DIK_ADD,
   NumpadMinus = DIK_SUBTRACT,
   NumpadMultiply = DIK_MULTIPLY,
   NumpadDivide = DIK_DIVIDE,
   NumpadDecimal = DIK_DECIMAL,

   // 記号
   Minus = DIK_MINUS,
   Equals = DIK_EQUALS,
   LeftBracket = DIK_LBRACKET,
   RightBracket = DIK_RBRACKET,
   Semicolon = DIK_SEMICOLON,
   Apostrophe = DIK_APOSTROPHE,
   Grave = DIK_GRAVE,
   Backslash = DIK_BACKSLASH,
   Comma = DIK_COMMA,
   Period = DIK_PERIOD,
   Slash = DIK_SLASH
};

/// @brief マウスボタン列挙型
enum class MouseButton : uint8_t {
   Left = 0,
   Right = 1,
   Middle = 2
};

/// @brief ゲームパッドボタン列挙型
enum class GamePadButton : WORD {
   DPadUp = XINPUT_GAMEPAD_DPAD_UP,
   DPadDown = XINPUT_GAMEPAD_DPAD_DOWN,
   DPadLeft = XINPUT_GAMEPAD_DPAD_LEFT,
   DPadRight = XINPUT_GAMEPAD_DPAD_RIGHT,
   Start = XINPUT_GAMEPAD_START,
   Back = XINPUT_GAMEPAD_BACK,
   LeftThumb = XINPUT_GAMEPAD_LEFT_THUMB,
   RightThumb = XINPUT_GAMEPAD_RIGHT_THUMB,
   LeftShoulder = XINPUT_GAMEPAD_LEFT_SHOULDER,
   RightShoulder = XINPUT_GAMEPAD_RIGHT_SHOULDER,
   A = XINPUT_GAMEPAD_A,
   B = XINPUT_GAMEPAD_B,
   X = XINPUT_GAMEPAD_X,
   Y = XINPUT_GAMEPAD_Y
};

/// @brief 入力管理クラス
class Input {
public:
   ~Input();

   /// @brief 入力システムの初期化
   /// @param hInstance アプリケーションのインスタンスハンドル
   /// @param hwnd ウィンドウハンドル
   void Initialize(HINSTANCE hInstance, HWND hwnd);

   /// @brief 入力システムの更新処
   void Update();

   /// @brief キーが押されているか
   /// @param key キーコード（DIK_で始まる値）
   /// @return キーが押されている場合はtrue
   bool IsKeyPressed(uint8_t key);

   /// @brief キーが押されているか（enum版）
   /// @param key キーコード
   /// @return キーが押されている場合はtrue
   bool IsKeyPressed(KeyCode key);

   /// @brief キーが押されていないか
   /// @param key キーコード（DIK_で始まる値）
   /// @return キーが押されていない場合はtrue
   bool IsKeyNotPressed(uint8_t key);

   /// @brief キーが押されていないか（enum版）
   /// @param key キーコード
   /// @return キーが押されていない場合はtrue
   bool IsKeyNotPressed(KeyCode key);

   /// @brief キーがトリガー(押された瞬間)されたか
   /// @param key キーコード（DIK_で始まる値）
   /// @return キーがトリガーされた場合はtrue
   bool IsKeyTriggered(uint8_t key);

   /// @brief キーがトリガー(押された瞬間)されたか（enum版）
   /// @param key キーコード
   /// @return キーがトリガーされた場合はtrue
   bool IsKeyTriggered(KeyCode key);

   /// @brief キーがリリース(離された瞬間)されたか
   /// @param key キーコード（DIK_で始まる値）
   /// @return キーがリリースされた場合はtrue
   bool IsKeyReleased(uint8_t key);

   /// @brief キーがリリース(離された瞬間)されたか（enum版）
   /// @param key キーコード
   /// @return キーがリリースされた場合はtrue
   bool IsKeyReleased(KeyCode key);

   /// @brief マウスのボタンが押されているか
   /// @param button マウスボタン（0:左, 1:右, 2:中ボタン）
   /// @return マウスボタンが押されている場合はtrue
   bool IsMousePressed(uint8_t button);

   /// @brief マウスのボタンが押されているか（enum版）
   /// @param button マウスボタン
   /// @return マウスボタンが押されている場合はtrue
   bool IsMousePressed(MouseButton button);

   /// @brief マウスのボタンが押されていないか
   /// @param button マウスボタン（0:左, 1:右, 2:中ボタン）
   /// @return マウスボタンが押されていない場合はtrue
   bool IsMouseNotPressed(uint8_t button);

   /// @brief マウスのボタンが押されていないか（enum版）
   /// @param button マウスボタン
   /// @return マウスボタンが押されていない場合はtrue
   bool IsMouseNotPressed(MouseButton button);

   /// @brief マウスのボタンがリリース(離された瞬間)されたか
   /// @param button マウスボタン（0:左, 1:右, 2:中ボタン）
   /// @return マウスボタンがリリースされた場合はtrue
   bool IsMouseReleased(uint8_t button);

   /// @brief マウスのボタンがリリース(離された瞬間)されたか（enum版）
   /// @param button マウスボタン
   /// @return マウスボタンがリリースされた場合はtrue
   bool IsMouseReleased(MouseButton button);

   /// @brief マウスのボタンがトリガー(押された瞬間)されたか
   /// @param button マウスボタン（0:左, 1:右, 2:中ボタン）
   /// @return マウスボタンがトリガーされた場合はtrue
   bool IsMouseTriggered(uint8_t button);

   /// @brief マウスのボタンがトリガー(押された瞬間)されたか（enum版）
   /// @param button マウスボタン
   /// @return マウスボタンがトリガーされた場合はtrue
   bool IsMouseTriggered(MouseButton button);

   /// @brief マウスのスクリーン座標を取得
   /// @return マウスのスクリーン座標
   Vector2 GetMouseScreenPosition() const;

   /// @brief マウスのスクリーン座標を取得
   /// @return マウスのスクリーン座標
   Vector2 GetMouseDelta() const;

   /// @brief マウスのホイールの回転量を取得
   /// @return マウスホイールの回転量
   int32_t GetMouseWheelDelta() const;

   /// @brief ゲームパッドが接続されているか
   /// @param index ゲームパッド番号（通常0）
   /// @return 接続されていればtrue
   bool IsGamePadConnected(uint32_t index = 0) const;

   /// @brief 接続されているゲームパッドの数を取得
   /// @return 現在接続されているゲームパッドの数（最大4）
   uint32_t GetConnectedGamePadCount() const;

   /// @brief ゲームパッドのボタンが押されていないか
   /// @param button ゲームパッドのボタンコード（XINPUT_GAMEPAD_で始まる値）
   /// @param index ゲームパッド番号（通常0）
   /// @return ボタンが押されていない場合はtrue
   bool IsGamePadButtonNotPressed(WORD button, uint32_t index = 0) const;

   /// @brief ゲームパッドのボタンが押されていないか（enum版）
   /// @param button ゲームパッドのボタン
   /// @param index ゲームパッド番号（通常0）
   /// @return ボタンが押されていない場合はtrue
   bool IsGamePadButtonNotPressed(GamePadButton button, uint32_t index = 0) const;

   /// @brief ゲームパッドのボタンが押されているか
   /// @param button ゲームパッドのボタンコード（XINPUT_GAMEPAD_で始まる値）
   /// @param index ゲームパッド番号（通常0）
   /// @return ボタンが押されている場合はtrue
   bool IsGamePadButtonPressed(WORD button, uint32_t index = 0) const;

   /// @brief ゲームパッドのボタンが押されているか（enum版）
   /// @param button ゲームパッドのボタン
   /// @param index ゲームパッド番号（通常0）
   /// @return ボタンが押されている場合はtrue
   bool IsGamePadButtonPressed(GamePadButton button, uint32_t index = 0) const;

   /// @brief ゲームパッドのボタンがトリガーされたか
   /// @param button ゲームパッドのボタンコード（XINPUT_GAMEPAD_で始まる値）
   /// @param index ゲームパッド番号（通常0）
   /// @return ボタンがトリガーされた場合はtrue
   bool IsGamePadButtonTriggered(WORD button, uint32_t index = 0) const;

   /// @brief ゲームパッドのボタンがトリガーされたか（enum版）
   /// @param button ゲームパッドのボタン
   /// @param index ゲームパッド番号（通常0）
   /// @return ボタンがトリガーされた場合はtrue
   bool IsGamePadButtonTriggered(GamePadButton button, uint32_t index = 0) const;

   /// @brief ゲームパッドのボタンがリリースされたか
   /// @param button ゲームパッドのボタンコード（XINPUT_GAMEPAD_で始まる値）
   /// @param index ゲームパッド番号（通常0）
   /// @return ボタンがリリースされた場合はtrue
   bool IsGamePadButtonReleased(WORD button, uint32_t index = 0) const;

   /// @brief ゲームパッドのボタンがリリースされたか（enum版）
   /// @param button ゲームパッドのボタン
   /// @param index ゲームパッド番号（通常0）
   /// @return ボタンがリリースされた場合はtrue
   bool IsGamePadButtonReleased(GamePadButton button, uint32_t index = 0) const;

   /// @brief ゲームパッドのスティックの状態を取得
   /// @param index ゲームパッド番号（通常0）
   /// @param deadZone デッドゾーンの値（0.0～1.0）
   /// @return 左スティックのベクトル
   Vector2 GetLeftStick(uint32_t index, float deadZone = 0.24) const;

   /// @brief ゲームパッドの右スティックの状態を取得
   /// @param index ゲームパッド番号（通常0)
   /// @param deadZone デッドゾーンの値（0.0～1.0）
   /// @return 右スティックのベクトル
   Vector2 GetRightStick(uint32_t index, float deadZone = 0.26f) const;

   // Todo: ↓ゲームパッドのスティックの状態を取得する関数は、デッドゾーンの値が少しバグっている。

   /// @brief ゲームパッドの左トリガーの状態を取得
   /// @param index ゲームパッド番号（通常0）
   /// @param deadZone デッドゾーンの値（0.0～1.0）
   /// @return 左トリガーの値（0.0～1.0）
   float GetLeftTrigger(uint32_t index, float deadZone = 0.12f) const;

   /// @brief ゲームパッドの右トリガーの状態を取得
   /// @param index ゲームパッド番号（通常0）
   /// @param deadZone デッドゾーンの値（0.0～1.0）
   /// @return 右トリガーの値（0.0～1.0）
   float GetRightTrigger(uint32_t index, float deadZone = 0.12f) const;

   /// @brief ゲームパッドの振動を設定
   /// @param index ゲームパッド番号（通常0）
   /// @param leftMotor 左モーターの強さ（0.0f～1.0f）
   /// @param rightMotor 右モーターの強さ（0.0f～1.0f）
   void SetVibration(uint32_t index, float leftMotor, float rightMotor);


private:
   ComPtr<IDirectInput8> directInput_ = nullptr;
   ComPtr<IDirectInputDevice8> keyboard_ = nullptr;
   BYTE key_[256] = {};
   BYTE prevKey_[256] = {};

   ComPtr<IDirectInputDevice8> mouse_ = nullptr;
   DIMOUSESTATE mouseState_ = {};
   DIMOUSESTATE prevMouseState_ = {};

   XINPUT_STATE gamePadState_[4]{};
   XINPUT_STATE prevGamePadState_[4]{};

   HWND hwnd_ = nullptr;
private:
   /// @brief キーボードの状態を更新
   void KeyboardUpdate();

   /// @brief マウスの状態を更新
   void MouseUpdate();

   /// @brief ゲームパッドの状態を更新
   void GamePadUpdate();
};
}