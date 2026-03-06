#include "KeyConfig.h"
#include <fstream>
#include <filesystem>
#include <cmath>

namespace GameEngine {

KeyConfig::KeyConfig(std::string filePath)
   : filePath_(std::move(filePath)) {}

void KeyConfig::BindKey(const std::string& action, KeyCode key) {
   keyBindings_[action] = key;
}

void KeyConfig::BindMouse(const std::string& action, MouseButton button) {
   mouseBindings_[action] = button;
}

void KeyConfig::BindGamePad(const std::string& action, GamePadButton button) {
   gamePadBindings_[action] = button;
}

void KeyConfig::BindLeftStick(const std::string& action) {
   stickBindings_[action] = 1;
}

void KeyConfig::BindRightStick(const std::string& action) {
   stickBindings_[action] = 2;
}

void KeyConfig::Unbind(const std::string& action) {
   keyBindings_.erase(action);
   mouseBindings_.erase(action);
   gamePadBindings_.erase(action);
   stickBindings_.erase(action);
}

void KeyConfig::Clear() {
   keyBindings_.clear();
   mouseBindings_.clear();
   gamePadBindings_.clear();
   stickBindings_.clear();
   callbacks_.clear();
}

void KeyConfig::RegisterCallbacks(const std::string& action, const ActionCallbacks& callbacks) {
   callbacks_[action] = callbacks;
}

void KeyConfig::RegisterOnPressed(const std::string& action, std::function<void()> cb) {
   callbacks_[action].onPressed = std::move(cb);
}

void KeyConfig::RegisterOnTriggered(const std::string& action, std::function<void()> cb) {
   callbacks_[action].onTriggered = std::move(cb);
}

void KeyConfig::RegisterOnReleased(const std::string& action, std::function<void()> cb) {
   callbacks_[action].onReleased = std::move(cb);
}

void KeyConfig::RemoveCallbacks(const std::string& action) {
   callbacks_.erase(action);
}

std::optional<KeyCode> KeyConfig::GetKeyBinding(const std::string& action) const {
   if (auto it = keyBindings_.find(action); it != keyBindings_.end()) return it->second;
   return std::nullopt;
}

std::optional<MouseButton> KeyConfig::GetMouseBinding(const std::string& action) const {
   if (auto it = mouseBindings_.find(action); it != mouseBindings_.end()) return it->second;
   return std::nullopt;
}

std::optional<GamePadButton> KeyConfig::GetGamePadBinding(const std::string& action) const {
   if (auto it = gamePadBindings_.find(action); it != gamePadBindings_.end()) return it->second;
   return std::nullopt;
}

int KeyConfig::GetStickBinding(const std::string& action) const {
   if (auto it = stickBindings_.find(action); it != stickBindings_.end()) {
	  return it->second;
   }
   return 0;
}

bool KeyConfig::IsPressed(const std::string& action, uint32_t gamePadIndex) const {
   if (auto k = GetKeyBinding(action)) {
	  if (EngineContext::IsKeyPressed(*k)) return true;
   }
   if (auto m = GetMouseBinding(action)) {
	  if (EngineContext::IsMousePressed(*m)) return true;
   }
   if (auto g = GetGamePadBinding(action)) {
	  if (EngineContext::IsGamePadButtonPressed(static_cast<WORD>(*g), gamePadIndex)) return true;
   }
   // スティックもチェック（しきい値0.5でbool判定）
   if (IsStickActive(action, gamePadIndex, 0.5f)) {
	  return true;
   }
   return false;
}

bool KeyConfig::IsTriggered(const std::string& action, uint32_t gamePadIndex) const {
   if (auto k = GetKeyBinding(action)) {
	  if (EngineContext::IsKeyTriggered(*k)) return true;
   }
   if (auto m = GetMouseBinding(action)) {
	  if (EngineContext::IsMouseTriggered(*m)) return true;
   }
   if (auto g = GetGamePadBinding(action)) {
	  if (EngineContext::IsGamePadButtonTriggered(static_cast<WORD>(*g), gamePadIndex)) return true;
   }
   // スティックのトリガー判定は複雑なので、ここでは対応しない
   return false;
}

bool KeyConfig::IsReleased(const std::string& action, uint32_t gamePadIndex) const {
   if (auto k = GetKeyBinding(action)) {
	  if (EngineContext::IsKeyReleased(*k)) return true;
   }
   if (auto m = GetMouseBinding(action)) {
	  if (EngineContext::IsMouseReleased(*m)) return true;
   }
   if (auto g = GetGamePadBinding(action)) {
	  if (EngineContext::IsGamePadButtonReleased(static_cast<WORD>(*g), gamePadIndex)) return true;
   }
   // スティックのリリース判定は複雑なので、ここでは対応しない
   return false;
}

Vector2 KeyConfig::GetStickVector(const std::string& action, uint32_t gamePadIndex, float deadZone) const {
   int stickType = GetStickBinding(action);
   if (stickType == 1) {
	  // 左スティック
	  return EngineContext::GetLeftStick(gamePadIndex, deadZone);
   } else if (stickType == 2) {
	  // 右スティック
	  return EngineContext::GetRightStick(gamePadIndex, deadZone);
   }
   return Vector2(0.0f, 0.0f);
}

bool KeyConfig::IsStickActive(const std::string& action, uint32_t gamePadIndex, float threshold) const {
   Vector2 stickVec = GetStickVector(action, gamePadIndex);
   float length = std::sqrt(stickVec.x * stickVec.x + stickVec.y * stickVec.y);
   return length >= threshold;
}

void KeyConfig::Update(uint32_t gamePadIndex) const {
   // 全アクションについて評価し、登録されているコールバックを呼ぶ
   for (const auto& [action, cbs] : callbacks_) {
	  if (cbs.onTriggered && IsTriggered(action, gamePadIndex)) {
		 cbs.onTriggered();
	  }
	  if (cbs.onReleased && IsReleased(action, gamePadIndex)) {
		 cbs.onReleased();
	  }
	  if (cbs.onPressed && IsPressed(action, gamePadIndex)) {
		 cbs.onPressed();
	  }
   }
}

bool KeyConfig::Load(const std::string& filePath) {
   std::string path = filePath.empty() ? filePath_ : filePath;
   std::ifstream ifs(path);
   if (!ifs.is_open()) return false;

   nlohmann::json j;
   try {
	  ifs >> j;
	  FromJson(j);
	  filePath_ = path;
	  return true;
   }
   catch (...) {
	  return false;
   }
}

bool KeyConfig::Save(const std::string& filePath) const {
   std::string path = filePath.empty() ? filePath_ : filePath;
   // ディレクトリ生成
   try {
	  std::filesystem::create_directories(std::filesystem::path(path).parent_path());
   }
   catch (...) {
	  // 無視（親が無い/作れないケース）
   }

   std::ofstream ofs(path);
   if (!ofs.is_open()) return false;

   try {
	  ofs << ToJson().dump(2);
	  return true;
   }
   catch (...) {
	  return false;
   }
}

void KeyConfig::SetDefaultBindings() {
   // 例: よくあるデフォルト
   BindKey("MoveLeft", KeyCode::A);
   BindKey("MoveRight", KeyCode::D);
   BindKey("MoveUp", KeyCode::W);
   BindKey("MoveDown", KeyCode::S);

   BindKey("Jump", KeyCode::Space);
   BindMouse("Fire", MouseButton::Left);

   BindGamePad("Jump", GamePadButton::A);
   BindGamePad("Fire", GamePadButton::RightShoulder);

   // スティック入力の例
   BindLeftStick("Move");        // 移動用に左スティック
   BindRightStick("Look");       // カメラ操作用に右スティック
}

nlohmann::json KeyConfig::ToJson() const {
   nlohmann::json j;

   nlohmann::json keys = nlohmann::json::object();
   for (const auto& [action, key] : keyBindings_) {
	  keys[action] = static_cast<int>(key);
   }

   nlohmann::json mice = nlohmann::json::object();
   for (const auto& [action, btn] : mouseBindings_) {
	  mice[action] = static_cast<int>(btn);
   }

   nlohmann::json pads = nlohmann::json::object();
   for (const auto& [action, btn] : gamePadBindings_) {
	  pads[action] = static_cast<int>(btn);
   }

   nlohmann::json sticks = nlohmann::json::object();
   for (const auto& [action, stickType] : stickBindings_) {
	  sticks[action] = stickType;
   }

   j["keys"] = std::move(keys);
   j["mouse"] = std::move(mice);
   j["gamepad"] = std::move(pads);
   j["sticks"] = std::move(sticks);

   return j;
}

void KeyConfig::FromJson(const nlohmann::json& j) {
   keyBindings_.clear();
   mouseBindings_.clear();
   gamePadBindings_.clear();
   stickBindings_.clear();

   if (j.contains("keys") && j["keys"].is_object()) {
	  for (auto it = j["keys"].begin(); it != j["keys"].end(); ++it) {
		 keyBindings_[it.key()] = static_cast<KeyCode>(it.value().get<int>());
	  }
   }
   if (j.contains("mouse") && j["mouse"].is_object()) {
	  for (auto it = j["mouse"].begin(); it != j["mouse"].end(); ++it) {
		 mouseBindings_[it.key()] = static_cast<MouseButton>(it.value().get<int>());
	  }
   }
   if (j.contains("gamepad") && j["gamepad"].is_object()) {
	  for (auto it = j["gamepad"].begin(); it != j["gamepad"].end(); ++it) {
		 gamePadBindings_[it.key()] = static_cast<GamePadButton>(it.value().get<int>());
	  }
   }
   if (j.contains("sticks") && j["sticks"].is_object()) {
	  for (auto it = j["sticks"].begin(); it != j["sticks"].end(); ++it) {
		 stickBindings_[it.key()] = it.value().get<int>();
	  }
   }
}

} // namespace GameEngine
