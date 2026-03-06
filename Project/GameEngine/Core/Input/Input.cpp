#include "pch.h"
#include "Input.h"

#pragma comment(lib,"dinput8.lib")
#pragma comment(lib,"dxguid.lib")
#pragma comment(lib, "xinput.lib")

namespace GameEngine {
Input::~Input() {
   for (size_t i = 0; i < GetConnectedGamePadCount(); ++i) {
	  SetVibration(static_cast<int32_t>(i), 0.0f, 0.0f);
   }
}

void Input::Initialize(HINSTANCE hInstance, HWND hwnd) {
   HRESULT result = DirectInput8Create(
	  hInstance,
	  DIRECTINPUT_VERSION,
	  IID_IDirectInput8,
	  reinterpret_cast<void**>(directInput_.GetAddressOf()),
	  nullptr
   );

   assert(SUCCEEDED(result));

   // キーボードデバイスの初期化
   result = directInput_->CreateDevice(GUID_SysKeyboard, &keyboard_, NULL);
   assert(SUCCEEDED(result));

   result = keyboard_->SetDataFormat(&c_dfDIKeyboard);
   assert(SUCCEEDED(result));

   result = keyboard_->SetCooperativeLevel(
	  hwnd,
	  DISCL_FOREGROUND | DISCL_NONEXCLUSIVE
   );

   // マウスデバイスの初期化
   result = directInput_->CreateDevice(GUID_SysMouse, &mouse_, NULL);
   assert(SUCCEEDED(result));

   result = mouse_->SetDataFormat(&c_dfDIMouse);
   assert(SUCCEEDED(result));

   result = mouse_->SetCooperativeLevel(
	  hwnd,
	  DISCL_FOREGROUND | DISCL_NONEXCLUSIVE
   );

   hwnd_ = hwnd;
}

void Input::Update() {
   KeyboardUpdate();
   MouseUpdate();
   GamePadUpdate();
}

void Input::KeyboardUpdate() {
   memcpy(prevKey_, key_, sizeof(key_));
   std::fill(std::begin(key_), std::end(key_), static_cast<BYTE>(0));
   keyboard_->Acquire();
   keyboard_->GetDeviceState(sizeof(key_), key_);
}

void Input::MouseUpdate() {
   prevMouseState_ = mouseState_;
   ZeroMemory(&mouseState_, sizeof(mouseState_));
   mouse_->Acquire();
   mouse_->GetDeviceState(sizeof(mouseState_), &mouseState_);
}

void Input::GamePadUpdate() {
   for (uint32_t i = 0; i < 4; ++i) {
	  prevGamePadState_[i] = gamePadState_[i];

	  ZeroMemory(&gamePadState_[i], sizeof(XINPUT_STATE));
	  XInputGetState(i, &gamePadState_[i]);
   }
}

bool Input::IsKeyPressed(uint8_t key) {
   return (key_[key] & 0x80) != 0;
}

bool Input::IsKeyPressed(KeyCode key) {
   return IsKeyPressed(static_cast<uint8_t>(key));
}

bool Input::IsKeyNotPressed(uint8_t key) {
   return (key_[key] & 0x80) == 0;
}

bool Input::IsKeyNotPressed(KeyCode key) {
   return IsKeyNotPressed(static_cast<uint8_t>(key));
}

bool Input::IsKeyTriggered(uint8_t key) {
   return (key_[key] & 0x80) != 0 && (prevKey_[key] & 0x80) == 0;
}

bool Input::IsKeyTriggered(KeyCode key) {
   return IsKeyTriggered(static_cast<uint8_t>(key));
}

bool Input::IsKeyReleased(uint8_t key) {
   return (key_[key] & 0x80) == 0 && (prevKey_[key] & 0x80) != 0;
}

bool Input::IsKeyReleased(KeyCode key) {
   return IsKeyReleased(static_cast<uint8_t>(key));
}

bool Input::IsMousePressed(uint8_t button) {
   return (mouseState_.rgbButtons[button] & 0x80) != 0;
}

bool Input::IsMousePressed(MouseButton button) {
   return IsMousePressed(static_cast<uint8_t>(button));
}

bool Input::IsMouseNotPressed(uint8_t button) {
   return (mouseState_.rgbButtons[button] & 0x80) == 0;
}

bool Input::IsMouseNotPressed(MouseButton button) {
   return IsMouseNotPressed(static_cast<uint8_t>(button));
}

bool Input::IsMouseTriggered(uint8_t button) {
   return (mouseState_.rgbButtons[button] & 0x80) != 0 &&
	  (prevMouseState_.rgbButtons[button] & 0x80) == 0;
}

bool Input::IsMouseTriggered(MouseButton button) {
   return IsMouseTriggered(static_cast<uint8_t>(button));
}

bool Input::IsMouseReleased(uint8_t button) {
   return (mouseState_.rgbButtons[button] & 0x80) == 0 &&
	  (prevMouseState_.rgbButtons[button] & 0x80) != 0;
}

bool Input::IsMouseReleased(MouseButton button) {
   return IsMouseReleased(static_cast<uint8_t>(button));
}

Vector2 Input::GetMouseScreenPosition() const {
   POINT pt{};
   GetCursorPos(&pt);
   ScreenToClient(hwnd_, &pt);
   return Vector2{ static_cast<float>(pt.x), static_cast<float>(pt.y) };
}

Vector2 Input::GetMouseDelta() const {
   return Vector2{
	   static_cast<float>(mouseState_.lX),
	   static_cast<float>(mouseState_.lY)
   };
}

int32_t Input::GetMouseWheelDelta() const {
   return static_cast<int32_t>(mouseState_.lZ);
}

bool Input::IsGamePadConnected(uint32_t index) const {
   XINPUT_STATE tempState{};
   return (index < 4) && (XInputGetState(index, &tempState) == ERROR_SUCCESS);
}

uint32_t Input::GetConnectedGamePadCount() const {
   uint32_t count = 0;

   for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i) {
	  XINPUT_STATE state = {};
	  DWORD result = XInputGetState(i, &state);
	  if (result == ERROR_SUCCESS) {
		 ++count;
	  }
   }

   return count;
}

bool Input::IsGamePadButtonPressed(WORD button, uint32_t index) const {
   return (index < 4) && ((gamePadState_[index].Gamepad.wButtons & button) != 0);
}

bool Input::IsGamePadButtonPressed(GamePadButton button, uint32_t index) const {
   return IsGamePadButtonPressed(static_cast<WORD>(button), index);
}

bool Input::IsGamePadButtonNotPressed(WORD button, uint32_t index) const {
   return (index < 4) && ((gamePadState_[index].Gamepad.wButtons & button) == 0);
}

bool Input::IsGamePadButtonNotPressed(GamePadButton button, uint32_t index) const {
   return IsGamePadButtonNotPressed(static_cast<WORD>(button), index);
}

bool Input::IsGamePadButtonTriggered(WORD button, uint32_t index) const {
   return (index < 4) &&
	  (gamePadState_[index].Gamepad.wButtons & button) &&
	  !(prevGamePadState_[index].Gamepad.wButtons & button);
}

bool Input::IsGamePadButtonTriggered(GamePadButton button, uint32_t index) const {
   return IsGamePadButtonTriggered(static_cast<WORD>(button), index);
}

bool Input::IsGamePadButtonReleased(WORD button, uint32_t index) const {
   return (index < 4) &&
	  !(gamePadState_[index].Gamepad.wButtons & button) &&
	  (prevGamePadState_[index].Gamepad.wButtons & button);
}

bool Input::IsGamePadButtonReleased(GamePadButton button, uint32_t index) const {
   return IsGamePadButtonReleased(static_cast<WORD>(button), index);
}

Vector2 Input::GetLeftStick(uint32_t index, float deadZone) const {
   if (index >= 4) return Vector2(0.0f, 0.0f);

   const auto& state = gamePadState_[index];
   float x = static_cast<float>(state.Gamepad.sThumbLX);
   float y = static_cast<float>(state.Gamepad.sThumbLY);

   float normX = x / 32767.0f;
   float normY = y / 32767.0f;
   float length = std::sqrt(normX * normX + normY * normY);

   if (length < deadZone) return Vector2(0.0f, 0.0f);

   float scale = (length - deadZone) / (1.0f - deadZone);
   scale = std::clamp(scale, 0.0f, 1.0f);

   return Vector2((normX / length) * scale, (normY / length) * scale);
}

Vector2 Input::GetRightStick(uint32_t index, float deadZone) const {
   if (index >= 4) return Vector2(0.0f, 0.0f);

   const auto& state = gamePadState_[index];
   float x = static_cast<float>(state.Gamepad.sThumbRX);
   float y = static_cast<float>(state.Gamepad.sThumbRY);

   float normX = x / 32767.0f;
   float normY = y / 32767.0f;
   float length = std::sqrt(normX * normX + normY * normY);

   if (length < deadZone) return Vector2(0.0f, 0.0f);

   float scale = (length - deadZone) / (1.0f - deadZone);
   scale = std::clamp(scale, 0.0f, 1.0f);

   return Vector2((normX / length) * scale, (normY / length) * scale);
}

float Input::GetLeftTrigger(uint32_t index, float deadZone) const {
   if (index >= 4) return 0.0f;
   float value = gamePadState_[index].Gamepad.bLeftTrigger / 255.0f;
   return (value < deadZone) ? 0.0f : (value - deadZone) / (1.0f - deadZone);
}

float Input::GetRightTrigger(uint32_t index, float deadZone) const {
   if (index >= 4) return 0.0f;
   float value = gamePadState_[index].Gamepad.bRightTrigger / 255.0f;
   return (value < deadZone) ? 0.0f : (value - deadZone) / (1.0f - deadZone);
}

void Input::SetVibration(uint32_t index, float leftMotor, float rightMotor) {
   if (index >= 4) return;

   // 0.0f～1.0f → 0～65535 に変換
   WORD leftValue = static_cast<WORD>(std::clamp(leftMotor, 0.0f, 1.0f) * 65535.0f);
   WORD rightValue = static_cast<WORD>(std::clamp(rightMotor, 0.0f, 1.0f) * 65535.0f);

   XINPUT_VIBRATION vibration{};
   vibration.wLeftMotorSpeed = leftValue;
   vibration.wRightMotorSpeed = rightValue;

   XInputSetState(index, &vibration);
}
}

