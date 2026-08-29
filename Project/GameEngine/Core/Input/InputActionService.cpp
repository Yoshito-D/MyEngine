#include "pch.h"
#include "InputActionService.h"
#include "Utility/Logger.h"
#include <fstream>
#include <nlohmann/json.hpp>

#ifdef USE_IMGUI
#include "imgui.h"
#endif

namespace GameEngine {
namespace {

using json = nlohmann::json;

constexpr int kInputActionFormatVersion = 1;
constexpr int kJsonIndentSize = 2;

const InputActionState kEmptyActionState{};

const std::pair<const char*, KeyCode> kKnownKeys[] = {
   { "A", KeyCode::A }, { "B", KeyCode::B }, { "C", KeyCode::C }, { "D", KeyCode::D },
   { "E", KeyCode::E }, { "F", KeyCode::F }, { "G", KeyCode::G }, { "H", KeyCode::H },
   { "I", KeyCode::I }, { "J", KeyCode::J }, { "K", KeyCode::K }, { "L", KeyCode::L },
   { "M", KeyCode::M }, { "N", KeyCode::N }, { "O", KeyCode::O }, { "P", KeyCode::P },
   { "Q", KeyCode::Q }, { "R", KeyCode::R }, { "S", KeyCode::S }, { "T", KeyCode::T },
   { "U", KeyCode::U }, { "V", KeyCode::V }, { "W", KeyCode::W }, { "X", KeyCode::X },
   { "Y", KeyCode::Y }, { "Z", KeyCode::Z },
   { "Num0", KeyCode::Num0 }, { "Num1", KeyCode::Num1 }, { "Num2", KeyCode::Num2 },
   { "Num3", KeyCode::Num3 }, { "Num4", KeyCode::Num4 }, { "Num5", KeyCode::Num5 },
   { "Num6", KeyCode::Num6 }, { "Num7", KeyCode::Num7 }, { "Num8", KeyCode::Num8 },
   { "Num9", KeyCode::Num9 },
   // F1はデバッグカメラ専用ショートカットのため再割り当て候補から除外する。
   { "F2", KeyCode::F2 }, { "F3", KeyCode::F3 }, { "F4", KeyCode::F4 },
   { "F5", KeyCode::F5 }, { "F6", KeyCode::F6 }, { "F7", KeyCode::F7 },
   { "F8", KeyCode::F8 }, { "F9", KeyCode::F9 }, { "F10", KeyCode::F10 },
   { "F11", KeyCode::F11 }, { "F12", KeyCode::F12 },
   { "Escape", KeyCode::Escape }, { "Tab", KeyCode::Tab }, { "CapsLock", KeyCode::CapsLock },
   { "LeftShift", KeyCode::LeftShift }, { "RightShift", KeyCode::RightShift },
   { "LeftCtrl", KeyCode::LeftCtrl }, { "RightCtrl", KeyCode::RightCtrl },
   { "LeftAlt", KeyCode::LeftAlt }, { "RightAlt", KeyCode::RightAlt },
   { "Space", KeyCode::Space }, { "Enter", KeyCode::Enter }, { "Backspace", KeyCode::Backspace },
   { "Delete", KeyCode::Delete }, { "Insert", KeyCode::Insert }, { "Home", KeyCode::Home },
   { "End", KeyCode::End }, { "PageUp", KeyCode::PageUp }, { "PageDown", KeyCode::PageDown },
   { "Up", KeyCode::Up }, { "Down", KeyCode::Down },
   { "Left", KeyCode::Left }, { "Right", KeyCode::Right },
   { "Numpad0", KeyCode::Numpad0 }, { "Numpad1", KeyCode::Numpad1 },
   { "Numpad2", KeyCode::Numpad2 }, { "Numpad3", KeyCode::Numpad3 },
   { "Numpad4", KeyCode::Numpad4 }, { "Numpad5", KeyCode::Numpad5 },
   { "Numpad6", KeyCode::Numpad6 }, { "Numpad7", KeyCode::Numpad7 },
   { "Numpad8", KeyCode::Numpad8 }, { "Numpad9", KeyCode::Numpad9 },
   { "NumpadEnter", KeyCode::NumpadEnter }, { "NumpadPlus", KeyCode::NumpadPlus },
   { "NumpadMinus", KeyCode::NumpadMinus }, { "NumpadMultiply", KeyCode::NumpadMultiply },
   { "NumpadDivide", KeyCode::NumpadDivide }, { "NumpadDecimal", KeyCode::NumpadDecimal },
   { "Minus", KeyCode::Minus }, { "Equals", KeyCode::Equals },
   { "LeftBracket", KeyCode::LeftBracket }, { "RightBracket", KeyCode::RightBracket },
   { "Semicolon", KeyCode::Semicolon }, { "Apostrophe", KeyCode::Apostrophe },
   { "Grave", KeyCode::Grave }, { "Backslash", KeyCode::Backslash },
   { "Comma", KeyCode::Comma }, { "Period", KeyCode::Period }, { "Slash", KeyCode::Slash }
};

const std::pair<const char*, GamePadButton> kKnownGamePadButtons[] = {
   { "DPadUp", GamePadButton::DPadUp }, { "DPadDown", GamePadButton::DPadDown },
   { "DPadLeft", GamePadButton::DPadLeft }, { "DPadRight", GamePadButton::DPadRight },
   { "Start", GamePadButton::Start }, { "Back", GamePadButton::Back },
   { "LeftThumb", GamePadButton::LeftThumb }, { "RightThumb", GamePadButton::RightThumb },
   { "A", GamePadButton::A }, { "B", GamePadButton::B },
   { "X", GamePadButton::X }, { "Y", GamePadButton::Y },
   { "LeftShoulder", GamePadButton::LeftShoulder },
   { "RightShoulder", GamePadButton::RightShoulder }
};

const char* ToString(InputActionType type) {
   switch (type) {
   case InputActionType::Button: return "Button";
   case InputActionType::Axis1D: return "Axis1D";
   case InputActionType::Axis2D: return "Axis2D";
   }
   return "Button";
}

bool TryParseActionType(const std::string& text, InputActionType& type) {
   if (text == "Button") { type = InputActionType::Button; return true; }
   if (text == "Axis1D") { type = InputActionType::Axis1D; return true; }
   if (text == "Axis2D") { type = InputActionType::Axis2D; return true; }
   return false;
}

const char* ToString(InputBindingType type) {
   switch (type) {
   case InputBindingType::Key: return "Key";
   case InputBindingType::GamePadButton: return "GamePadButton";
   case InputBindingType::KeyAxis1D: return "KeyAxis1D";
   case InputBindingType::KeyAxis2D: return "KeyAxis2D";
   case InputBindingType::LeftStick: return "LeftStick";
   case InputBindingType::RightStick: return "RightStick";
   }
   return "Key";
}

bool TryParseBindingType(const std::string& text, InputBindingType& type) {
   if (text == "Key") { type = InputBindingType::Key; return true; }
   if (text == "GamePadButton") { type = InputBindingType::GamePadButton; return true; }
   if (text == "KeyAxis1D") { type = InputBindingType::KeyAxis1D; return true; }
   if (text == "KeyAxis2D") { type = InputBindingType::KeyAxis2D; return true; }
   if (text == "LeftStick") { type = InputBindingType::LeftStick; return true; }
   if (text == "RightStick") { type = InputBindingType::RightStick; return true; }
   return false;
}

const char* ToString(InputBindingAxis axis) {
   switch (axis) {
   case InputBindingAxis::Both: return "Both";
   case InputBindingAxis::X: return "X";
   case InputBindingAxis::Y: return "Y";
   }
   return "Both";
}

bool TryParseBindingAxis(const std::string& text, InputBindingAxis& axis) {
   if (text == "Both") { axis = InputBindingAxis::Both; return true; }
   if (text == "X") { axis = InputBindingAxis::X; return true; }
   if (text == "Y") { axis = InputBindingAxis::Y; return true; }
   return false;
}

const char* ToString(KeyCode key) {
   for (const auto& [name, value] : kKnownKeys) {
      if (value == key) { return name; }
   }
   return "Space";
}

bool TryParseKey(const std::string& text, KeyCode& key) {
   for (const auto& [name, value] : kKnownKeys) {
      if (text == name) { key = value; return true; }
   }
   return false;
}

const char* ToString(GamePadButton button) {
   for (const auto& [name, value] : kKnownGamePadButtons) {
      if (value == button) { return name; }
   }
   return "A";
}

bool TryParseGamePadButton(const std::string& text, GamePadButton& button) {
   for (const auto& [name, value] : kKnownGamePadButtons) {
      if (text == name) { button = value; return true; }
   }
   return false;
}

json SerializeBinding(const InputBinding& binding) {
   // バインド種別ごとに有効なフィールドだけを書き出す。未使用メンバーまで保存すると、
   // 後から種別を変更した際に古い値を誤って復元する可能性があるためである。
   json result;
   result["type"] = ToString(binding.type);
   result["scale"] = binding.scale;
   switch (binding.type) {
   case InputBindingType::Key:
      result["key"] = ToString(binding.key);
      break;
   case InputBindingType::GamePadButton:
      result["button"] = ToString(binding.gamePadButton);
      break;
   case InputBindingType::KeyAxis1D:
      result["negative"] = ToString(binding.negativeKey);
      result["positive"] = ToString(binding.positiveKey);
      break;
   case InputBindingType::KeyAxis2D:
      result["left"] = ToString(binding.leftKey);
      result["right"] = ToString(binding.rightKey);
      result["up"] = ToString(binding.upKey);
      result["down"] = ToString(binding.downKey);
      break;
   case InputBindingType::LeftStick:
   case InputBindingType::RightStick:
      result["axis"] = ToString(binding.axis);
      break;
   }
   return result;
}

bool DeserializeBinding(const json& source, InputBinding& binding) {
   // 設定ファイルはユーザーが直接編集できるため、値を取り出す前に型まで検証する。
   // 一つでも必須項目が壊れていれば部分的なバインドを作らず、呼び出し側で既定値を維持する。
   if (!source.is_object() || !source.contains("type") || !source["type"].is_string()) { return false; }
   if (!TryParseBindingType(source["type"].get<std::string>(), binding.type)) { return false; }
   if (source.contains("scale") && !source.at("scale").is_number()) { return false; }
   binding.scale = source.contains("scale") ? source.at("scale").get<float>() : 1.0f;

   switch (binding.type) {
   case InputBindingType::Key:
      return source.contains("key") && source["key"].is_string() &&
         TryParseKey(source["key"].get<std::string>(), binding.key);
   case InputBindingType::GamePadButton:
      return source.contains("button") && source["button"].is_string() &&
         TryParseGamePadButton(source["button"].get<std::string>(), binding.gamePadButton);
   case InputBindingType::KeyAxis1D:
      return source.contains("negative") && source.at("negative").is_string() &&
         source.contains("positive") && source.at("positive").is_string() &&
         TryParseKey(source["negative"].get<std::string>(), binding.negativeKey) &&
         TryParseKey(source["positive"].get<std::string>(), binding.positiveKey);
   case InputBindingType::KeyAxis2D:
      return source.contains("left") && source.at("left").is_string() &&
         source.contains("right") && source.at("right").is_string() &&
         source.contains("up") && source.at("up").is_string() &&
         source.contains("down") && source.at("down").is_string() &&
         TryParseKey(source["left"].get<std::string>(), binding.leftKey) &&
         TryParseKey(source["right"].get<std::string>(), binding.rightKey) &&
         TryParseKey(source["up"].get<std::string>(), binding.upKey) &&
         TryParseKey(source["down"].get<std::string>(), binding.downKey);
   case InputBindingType::LeftStick:
   case InputBindingType::RightStick:
      if (!source.contains("axis")) {
         // axis が存在しない旧形式も読み込めるよう、従来どおりスティック全体を割り当てる。
         binding.axis = InputBindingAxis::Both;
         return true;
      }
      return source.at("axis").is_string() &&
         TryParseBindingAxis(source.at("axis").get<std::string>(), binding.axis);
   }
   return false;
}

std::string DescribeBinding(const InputBinding& binding) {
   switch (binding.type) {
   case InputBindingType::Key: return std::string("Key ") + ToString(binding.key);
   case InputBindingType::GamePadButton: return std::string("GamePad ") + ToString(binding.gamePadButton);
   case InputBindingType::KeyAxis1D:
      return std::string(ToString(binding.negativeKey)) + " / " + ToString(binding.positiveKey);
   case InputBindingType::KeyAxis2D:
      return std::string(ToString(binding.leftKey)) + " / " + ToString(binding.rightKey) + " / " +
         ToString(binding.upKey) + " / " + ToString(binding.downKey);
   case InputBindingType::LeftStick:
      return std::string("Left Stick ") + ToString(binding.axis);
   case InputBindingType::RightStick:
      return std::string("Right Stick ") + ToString(binding.axis);
   }
   return {};
}

enum class PhysicalControlType {
   Key,
   GamePadButton,
   LeftStick,
   RightStick,
};

struct PhysicalControl {
   PhysicalControlType type;
   uint32_t value;

   bool operator==(const PhysicalControl&) const = default;
};

std::vector<PhysicalControl> GetPhysicalControls(const InputBinding& binding) {
   // 複合バインドを「物理デバイス上の入力」の列へ正規化することで、
   // KeyAxis1D/2D を含む異種バインド同士でも同じ手順で競合を検出できる。
   auto key = [](KeyCode value) {
      return PhysicalControl{ PhysicalControlType::Key, static_cast<uint32_t>(value) };
   };
   switch (binding.type) {
      case InputBindingType::Key:
         return { key(binding.key) };
      case InputBindingType::GamePadButton:
         return { { PhysicalControlType::GamePadButton, static_cast<uint32_t>(binding.gamePadButton) } };
      case InputBindingType::KeyAxis1D:
         return { key(binding.negativeKey), key(binding.positiveKey) };
      case InputBindingType::KeyAxis2D:
         return { key(binding.leftKey), key(binding.rightKey), key(binding.upKey), key(binding.downKey) };
      case InputBindingType::LeftStick:
         return { { PhysicalControlType::LeftStick, 0 } };
      case InputBindingType::RightStick:
         return { { PhysicalControlType::RightStick, 0 } };
   }
   return {};
}

bool SharesPhysicalControl(const InputBinding& left, const InputBinding& right) {
   const auto leftControls = GetPhysicalControls(left);
   const auto rightControls = GetPhysicalControls(right);
   return std::any_of(leftControls.begin(), leftControls.end(), [&rightControls](const PhysicalControl& control) {
      return std::find(rightControls.begin(), rightControls.end(), control) != rightControls.end();
   });
}

bool FindSingleChangedControl(
   const InputBinding& previous,
   const InputBinding& replacement,
   PhysicalControl& previousControl,
   PhysicalControl& replacementControl) {
   // 競合時に安全に入れ替えられるのは、同じ形のバインドで一つの物理入力だけを
   // 変更した場合に限る。複数変更を推測で交換すると別方向の割り当てまで壊れてしまう。
   if (previous.type != replacement.type) {
      return false;
   }
   const auto previousControls = GetPhysicalControls(previous);
   const auto replacementControls = GetPhysicalControls(replacement);
   if (previousControls.size() != replacementControls.size()) {
      return false;
   }

   size_t changedIndex = previousControls.size();
   for (size_t index = 0; index < previousControls.size(); ++index) {
      if (previousControls[index] != replacementControls[index]) {
         if (changedIndex != previousControls.size()) {
            return false;
         }
         changedIndex = index;
      }
   }
   if (changedIndex == previousControls.size()) {
      return false;
   }
   previousControl = previousControls[changedIndex];
   replacementControl = replacementControls[changedIndex];
   return true;
}

bool ReplacePhysicalControl(
   InputBinding& binding,
   const PhysicalControl& target,
   const PhysicalControl& replacement) {
   // キーとゲームパッドボタンのようにデバイス種別をまたぐ置換は行わない。
   // 呼び出し側は false を受け取ると、バインド全体を旧割り当てへ差し替える。
   if (target.type != replacement.type) {
      return false;
   }
   bool replaced = false;
   auto replaceKey = [&](KeyCode& key) {
      if (target.type == PhysicalControlType::Key && static_cast<uint32_t>(key) == target.value) {
         key = static_cast<KeyCode>(replacement.value);
         replaced = true;
      }
   };
   switch (binding.type) {
      case InputBindingType::Key: replaceKey(binding.key); break;
      case InputBindingType::GamePadButton:
         if (target.type == PhysicalControlType::GamePadButton &&
            static_cast<uint32_t>(binding.gamePadButton) == target.value) {
            binding.gamePadButton = static_cast<GamePadButton>(replacement.value);
            replaced = true;
         }
         break;
      case InputBindingType::KeyAxis1D:
         replaceKey(binding.negativeKey);
         replaceKey(binding.positiveKey);
         break;
      case InputBindingType::KeyAxis2D:
         replaceKey(binding.leftKey);
         replaceKey(binding.rightKey);
         replaceKey(binding.upKey);
         replaceKey(binding.downKey);
         break;
      case InputBindingType::LeftStick:
      case InputBindingType::RightStick:
         break;
   }
   return replaced;
}

} // namespace

void InputActionService::Initialize(
   IInputBackend& backend,
   const std::filesystem::path& defaultPath,
   const std::filesystem::path& overridePath) {
   backend_ = &backend;
   defaultPath_ = defaultPath;
   overridePath_ = overridePath;

   // 組み込み定義を土台に外部定義を重ねる。ファイルが欠けても最低限の操作を失わず、
   // 正常に読めた定義だけを「リセット先」として確定してからユーザー設定を適用する。
   BuildFallbackDefaults();

   if (!LoadDefinitions(defaultPath_, true)) {
      Logger::Warning("Input actions default file is missing or invalid; built-in defaults are active: " + defaultPath_.generic_string());
   }
   defaults_ = actions_;
   LoadOverrides();
}

void InputActionService::BuildFallbackDefaults() {
   // 起動不能を避けるための最小構成。ここで定義した操作は既定JSONが壊れていても残る。
   actions_.clear();

   auto addAction = [this](const char* map, const char* id, InputActionType type, std::initializer_list<InputBinding> bindings) {
      ActionDefinition action;
      action.map = map;
      action.id = id;
      action.type = type;
      action.bindings.assign(bindings.begin(), bindings.end());
      actions_[MakeKey(map, id)] = std::move(action);
   };

   // 同じアクションへキーボードとゲームパッドを併記し、Update() 側で入力値を合成する。
   InputBinding steerKeys;
   steerKeys.type = InputBindingType::KeyAxis1D;
   steerKeys.negativeKey = KeyCode::A;
   steerKeys.positiveKey = KeyCode::D;
   InputBinding leftStick;
   leftStick.type = InputBindingType::LeftStick;
   leftStick.axis = InputBindingAxis::X;
   leftStick.scale = 1.0f;
   addAction("Gameplay", "Vehicle.Steer", InputActionType::Axis1D, { steerKeys, leftStick });

   InputBinding pitchKeys = steerKeys;
   pitchKeys.negativeKey = KeyCode::S;
   pitchKeys.positiveKey = KeyCode::W;
   InputBinding pitchStick = leftStick;
   pitchStick.axis = InputBindingAxis::Y;
   addAction("Gameplay", "Vehicle.Pitch", InputActionType::Axis1D, { pitchKeys, pitchStick });

   InputBinding rollKeys = steerKeys;
   rollKeys.negativeKey = KeyCode::Q;
   rollKeys.positiveKey = KeyCode::E;
   // 左右ショルダーは同じ1D軸へ逆符号で加算し、同時押しなら相殺されるようにする。
   InputBinding rollLeftShoulder;
   rollLeftShoulder.type = InputBindingType::GamePadButton;
   rollLeftShoulder.gamePadButton = GamePadButton::LeftShoulder;
   rollLeftShoulder.scale = -1.0f;
   InputBinding rollRightShoulder = rollLeftShoulder;
   rollRightShoulder.gamePadButton = GamePadButton::RightShoulder;
   rollRightShoulder.scale = 1.0f;
   addAction(
      "Gameplay",
      "Vehicle.Roll",
      InputActionType::Axis1D,
      { rollKeys, rollLeftShoulder, rollRightShoulder });

   InputBinding space;
   space.type = InputBindingType::Key;
   space.key = KeyCode::Space;
   InputBinding gamePadA;
   gamePadA.type = InputBindingType::GamePadButton;
   gamePadA.gamePadButton = GamePadButton::A;
   addAction("Gameplay", "Vehicle.Jump", InputActionType::Button, { space, gamePadA });

   InputBinding driftKey = space;
   driftKey.key = KeyCode::Q;
   InputBinding leftShoulder = gamePadA;
   leftShoulder.gamePadButton = GamePadButton::LeftShoulder;
   addAction("Gameplay", "Vehicle.Drift", InputActionType::Button, { driftKey, leftShoulder });

   InputBinding lookKeys;
   lookKeys.type = InputBindingType::KeyAxis2D;
   InputBinding rightStick;
   rightStick.type = InputBindingType::RightStick;
   rightStick.axis = InputBindingAxis::Both;
   addAction("Gameplay", "Camera.Look", InputActionType::Axis2D, { lookKeys, rightStick });

   InputBinding tab = space;
   tab.key = KeyCode::Tab;
   addAction("Gameplay", "Camera.Next", InputActionType::Button, { tab });

   InputBinding navigateKeys = steerKeys;
   navigateKeys.negativeKey = KeyCode::W;
   navigateKeys.positiveKey = KeyCode::S;
   InputBinding navigateUp = gamePadA;
   navigateUp.gamePadButton = GamePadButton::DPadUp;
   navigateUp.scale = -1.0f;
   InputBinding navigateDown = gamePadA;
   navigateDown.gamePadButton = GamePadButton::DPadDown;
   InputBinding navigateStick = leftStick;
   navigateStick.axis = InputBindingAxis::Y;
   navigateStick.scale = -1.0f;
   addAction(
      "UI",
      "UI.Navigate",
      InputActionType::Axis1D,
      { navigateKeys, navigateUp, navigateDown, navigateStick });
   addAction("UI", "UI.Confirm", InputActionType::Button, { space, gamePadA });
}

bool InputActionService::LoadDefinitions(const std::filesystem::path& path, bool replaceAll) {
   std::ifstream stream(path);
   if (!stream) { return false; }

   json root;
   try {
      stream >> root;
   } catch (const json::exception&) {
      return false;
   }
   if (!root.contains("maps") || !root["maps"].is_object()) { return false; }

   // 読み込み途中で不正項目が見つかっても現在の設定を壊さないよう、作業用コピーへ適用する。
   // replaceAll は未知アクションの追加可否を表し、override 読み込み時は既知項目だけを許可する。
   auto loadedActions = replaceAll ? actions_ : actions_;
   size_t appliedCount = 0;
   for (const auto& [mapName, mapData] : root["maps"].items()) {
      if (!mapData.is_object()) { continue; }
      for (const auto& [actionId, actionData] : mapData.items()) {
         if (!actionData.is_object()) { continue; }
         const std::string key = MakeKey(mapName, actionId);
         auto existing = actions_.find(key);
         if (!replaceAll && existing == actions_.end()) {
            Logger::Warning("Ignoring unknown input action override: " + mapName + "/" + actionId);
            continue;
         }

          // 各項目を既存定義から始めることで、不正な項目だけフォールバック値へ戻せる。
          ActionDefinition action = existing != actions_.end() ? existing->second : ActionDefinition{};
         action.map = mapName;
         action.id = actionId;
         InputActionType type = action.type;
         if (!actionData.contains("type") || !actionData["type"].is_string() ||
             !TryParseActionType(actionData["type"].get<std::string>(), type) ||
             !actionData.contains("bindings") || !actionData["bindings"].is_array()) {
            Logger::Warning("Invalid input action; retaining fallback: " + mapName + "/" + actionId);
            continue;
         }

          // 配列全体を検証してから置換する。途中まで有効な配列は操作体系を予測不能にするため採用しない。
          std::vector<InputBinding> bindings;
         bool valid = true;
         for (const json& bindingData : actionData["bindings"]) {
            InputBinding binding;
            if (!DeserializeBinding(bindingData, binding)) { valid = false; break; }
            bindings.push_back(binding);
         }
         if (!valid || bindings.empty()) {
            Logger::Warning("Invalid bindings; retaining fallback: " + mapName + "/" + actionId);
            continue;
         }

         if (actionData.contains("deadZone") && !actionData.at("deadZone").is_number()) {
            Logger::Warning("Invalid dead zone; retaining fallback: " + mapName + "/" + actionId);
            continue;
         }

         action.type = type;
         const float configuredDeadZone = actionData.contains("deadZone")
            ? actionData.at("deadZone").get<float>()
            : action.deadZone;
          // 1.0ではスティックが決して反応しなくなるため、上限にはわずかな余裕を残す。
          action.deadZone = std::clamp(configuredDeadZone, 0.0f, 0.95f);
         action.bindings = std::move(bindings);
         loadedActions[key] = std::move(action);
         ++appliedCount;
      }
   }

   if (replaceAll && appliedCount == 0) { return false; }
   actions_ = std::move(loadedActions);
   return true;
}

void InputActionService::Update() {
   if (!backend_) { return; }
   ++frameNumber_;

   // 全アクションをプレイヤー単位で評価し、このフレームのスナップショットを完成させる。
   // 利用側が同じフレームに何度問い合わせても triggered/released が変化しない設計である。
   for (auto& [key, action] : actions_) {
      (void)key;
      for (uint32_t playerSlot = 0; playerSlot < kMaxPlayers; ++playerSlot) {
         Vector2 value{};
         for (const InputBinding& binding : action.bindings) {
            const Vector2 bindingValue = EvaluateBinding(binding, playerSlot, action.deadZone);
            if (action.type == InputActionType::Button) {
               // ボタンは複数デバイスの論理OR。加算すると二重入力時だけ値が変わるため最大値を使う。
               value.x = std::max(value.x, bindingValue.x);
            } else {
               // 軸入力はキーボード、スティック、補助ボタンを合成できるよう加算する。
               value.x += bindingValue.x;
               value.y += bindingValue.y;
            }
         }

         // 複数バインドの合成後も、利用側へ渡す論理入力の範囲は常に [-1, 1] に保つ。
         value.x = std::clamp(value.x, -1.0f, 1.0f);
         value.y = std::clamp(value.y, -1.0f, 1.0f);
         InputActionState& state = action.states[playerSlot];
         // 前フレームの held と比較してエッジを生成する。バックエンドごとのトリガー判定へ
         // 依存しないため、キーボードとゲームパッドを同じアクションとして扱える。
         const bool wasHeld = state.held;
         state.value = value;
         state.held = std::abs(value.x) > 0.0001f || std::abs(value.y) > 0.0001f;
         state.triggered = state.held && !wasHeld;
         state.released = !state.held && wasHeld;
         state.frameNumber = frameNumber_;
      }
   }
}

Vector2 InputActionService::EvaluateBinding(const InputBinding& binding, uint32_t playerSlot, float deadZone) const {
   Vector2 value{};
   // キーボードは共有デバイスなのでプレイヤー0だけへ割り当てる。
   // ゲームパッドは playerSlot と物理ポートを一対一で対応させる。
   const bool allowKeyboard = playerSlot == 0;
   switch (binding.type) {
   case InputBindingType::Key:
      if (allowKeyboard && backend_->IsKeyPressed(binding.key)) { value.x = 1.0f; }
      break;
   case InputBindingType::GamePadButton:
      if (backend_->IsGamePadButtonPressed(binding.gamePadButton, playerSlot)) { value.x = 1.0f; }
      break;
   case InputBindingType::KeyAxis1D:
      if (allowKeyboard) {
         if (backend_->IsKeyPressed(binding.negativeKey)) { value.x -= 1.0f; }
         if (backend_->IsKeyPressed(binding.positiveKey)) { value.x += 1.0f; }
      }
      break;
   case InputBindingType::KeyAxis2D:
      if (allowKeyboard) {
         if (backend_->IsKeyPressed(binding.leftKey)) { value.x -= 1.0f; }
         if (backend_->IsKeyPressed(binding.rightKey)) { value.x += 1.0f; }
         if (backend_->IsKeyPressed(binding.upKey)) { value.y -= 1.0f; }
         if (backend_->IsKeyPressed(binding.downKey)) { value.y += 1.0f; }
      }
      break;
   case InputBindingType::LeftStick:
      value = backend_->GetLeftStick(playerSlot, deadZone);
      break;
   case InputBindingType::RightStick:
      value = backend_->GetRightStick(playerSlot, deadZone);
      break;
   }
   // 1Dアクションは最終的に x 成分を読むため、Y指定も x へ詰め替える。
   // Both は2D値を保ち、Axis2Dアクションからそのまま利用できる。
   if (binding.axis == InputBindingAxis::X) {
      value.y = 0.0f;
   } else if (binding.axis == InputBindingAxis::Y) {
      value.x = value.y;
      value.y = 0.0f;
   }
   value.x *= binding.scale;
   value.y *= binding.scale;
   return value;
}

const InputActionState& InputActionService::GetActionState(
   const std::string& actionMap,
   const std::string& actionId,
   uint32_t playerSlot) const {
   const ActionDefinition* action = FindAction(actionMap, actionId);
   if (!action || playerSlot >= kMaxPlayers) { return kEmptyActionState; }
   return action->states[playerSlot];
}

bool InputActionService::BeginRebind(const std::string& actionMap, const std::string& actionId, size_t bindingIndex) {
   return BeginRebindPart(actionMap, actionId, bindingIndex, RebindPart::Whole);
}

bool InputActionService::BeginRebindPart(
   const std::string& actionMap,
   const std::string& actionId,
   size_t bindingIndex,
   RebindPart part) {
   const ActionDefinition* action = FindAction(actionMap, actionId);
   if (!action || bindingIndex >= action->bindings.size()) { return false; }
   pendingRebind_ = { actionMap, actionId, bindingIndex, part, true };
   hasPendingConflict_ = false;
   return true;
}

bool InputActionService::ApplyBinding(
   const std::string& actionMap,
   const std::string& actionId,
   size_t bindingIndex,
   const InputBinding& binding,
   bool replaceConflict) {
   ActionDefinition* target = FindAction(actionMap, actionId);
   if (!target || bindingIndex >= target->bindings.size()) { return false; }
   const InputBinding previousTargetBinding = target->bindings[bindingIndex];

   // 一方向だけを再割り当てた複合軸では、競合先の同じ物理入力と旧入力を交換する。
   // これにより「左をDへ変更」した際、既存の右=Dが未割り当てになるのを防ぐ。
   PhysicalControl previousControl{};
   PhysicalControl replacementControl{};
   const bool changesSingleControl = FindSingleChangedControl(
      previousTargetBinding, binding, previousControl, replacementControl);

   // マップをまたいで同じ物理入力が二重登録されないよう、全アクションを走査する。
   for (auto& [key, action] : actions_) {
      (void)key;
      for (InputBinding& existing : action.bindings) {
         const bool isTarget = &action == target && &existing == &target->bindings[bindingIndex];
         if (!isTarget && SharesPhysicalControl(existing, binding)) {
            if (!replaceConflict) { return false; }
            if (!changesSingleControl ||
                !ReplacePhysicalControl(existing, replacementControl, previousControl)) {
               // 安全な局所交換ができない場合は、競合側へ対象の旧バインド全体を移す。
               existing = previousTargetBinding;
            }
         }
      }
   }
   target->bindings[bindingIndex] = binding;
   return true;
}

bool InputActionService::LoadOverrides() {
   if (!std::filesystem::exists(overridePath_)) {
      Logger::Warning("Input override file is missing; defaults are active: " + overridePath_.generic_string());
      return false;
   }
   if (!LoadDefinitions(overridePath_, false)) {
      Logger::Warning("Input override file is invalid; defaults are active per action: " + overridePath_.generic_string());
      return false;
   }
   return true;
}

bool InputActionService::SaveOverrides() const {
   // 現在有効な定義を完全なスナップショットとして保存し、既定JSONの変更後も
   // ユーザーが確認した割り当てが不用意に変わらないようにする。
   json root;
   root["version"] = kInputActionFormatVersion;
   for (const auto& [key, action] : actions_) {
      (void)key;
      json actionData;
      actionData["type"] = ToString(action.type);
      actionData["deadZone"] = action.deadZone;
      actionData["bindings"] = json::array();
      for (const InputBinding& binding : action.bindings) {
         actionData["bindings"].push_back(SerializeBinding(binding));
      }
      root["maps"][action.map][action.id] = std::move(actionData);
   }

   std::error_code error;
   std::filesystem::create_directories(overridePath_.parent_path(), error);

   // 直接上書きすると書き込み中の終了でJSONが破損するため、一時ファイルを閉じてから
   // WRITE_THROUGH付きの置換で切り替える。
   const std::filesystem::path temporaryPath = overridePath_.wstring() + L".tmp";
   std::ofstream stream(temporaryPath, std::ios::trunc);
   if (!stream) { return false; }
   stream << root.dump(kJsonIndentSize);
   stream.close();
   if (!stream) { return false; }

   if (!MoveFileExW(
      temporaryPath.c_str(),
      overridePath_.c_str(),
      MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
      std::filesystem::remove(temporaryPath, error);
      return false;
   }
   return true;
}

void InputActionService::ResetToDefaults() {
   actions_ = defaults_;
   pendingRebind_ = {};
   hasPendingConflict_ = false;
}

InputActionService::ActionDefinition* InputActionService::FindAction(
   const std::string& actionMap,
   const std::string& actionId) {
   auto it = actions_.find(MakeKey(actionMap, actionId));
   return it != actions_.end() ? &it->second : nullptr;
}

const InputActionService::ActionDefinition* InputActionService::FindAction(
   const std::string& actionMap,
   const std::string& actionId) const {
   auto it = actions_.find(MakeKey(actionMap, actionId));
   return it != actions_.end() ? &it->second : nullptr;
}

std::string InputActionService::MakeKey(const std::string& actionMap, const std::string& actionId) {
   // UI表示用文字列と衝突しにくい改行を区切りに使い、map/id の組を単一キーへまとめる。
   return actionMap + '\n' + actionId;
}

#ifdef USE_IMGUI
void InputActionService::DrawImGui() {
   if (!ImGui::CollapsingHeader("Input Actions")) { return; }
   ImGui::TextUnformatted("Keyboard/Mouse: Player 0, XInput: matching player slot");

   // actions_ はunordered_mapのため、そのまま描画すると毎回並びが揺れる。
   // ポインターだけを並べ替え、実体と再バインド状態のアドレスは維持する。
   std::vector<ActionDefinition*> sortedActions;
   sortedActions.reserve(actions_.size());
   for (auto& [key, action] : actions_) {
      (void)key;
      sortedActions.push_back(&action);
   }
   std::sort(sortedActions.begin(), sortedActions.end(), [](const ActionDefinition* left, const ActionDefinition* right) {
      return MakeKey(left->map, left->id) < MakeKey(right->map, right->id);
   });

   for (ActionDefinition* action : sortedActions) {
      ImGui::PushID(action->id.c_str());
      ImGui::Text("%s / %s (%s)", action->map.c_str(), action->id.c_str(), ToString(action->type));
      for (size_t index = 0; index < action->bindings.size(); ++index) {
         ImGui::PushID(static_cast<int>(index));
         const InputBinding& binding = action->bindings[index];
         ImGui::BulletText("%s", DescribeBinding(binding).c_str());
         if (binding.type == InputBindingType::KeyAxis1D) {
            ImGui::SameLine();
            if (ImGui::SmallButton("Negative")) {
               BeginRebindPart(action->map, action->id, index, RebindPart::Negative);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Positive")) {
               BeginRebindPart(action->map, action->id, index, RebindPart::Positive);
            }
         } else if (binding.type == InputBindingType::KeyAxis2D) {
            ImGui::SameLine();
            if (ImGui::SmallButton("Left")) {
               BeginRebindPart(action->map, action->id, index, RebindPart::Left);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Right")) {
               BeginRebindPart(action->map, action->id, index, RebindPart::Right);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Up")) {
               BeginRebindPart(action->map, action->id, index, RebindPart::Up);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Down")) {
               BeginRebindPart(action->map, action->id, index, RebindPart::Down);
            }
         } else if (binding.type == InputBindingType::Key || binding.type == InputBindingType::GamePadButton) {
            ImGui::SameLine();
            if (ImGui::SmallButton("Rebind")) { BeginRebind(action->map, action->id, index); }
         }
         ImGui::PopID();
      }
      ImGui::PopID();
   }

   if (pendingRebind_.active) {
      ImGui::Separator();
      ImGui::Text("Press an input for %s", pendingRebind_.actionId.c_str());
      ActionDefinition* pendingAction = FindAction(pendingRebind_.map, pendingRebind_.actionId);
      InputBinding captured = pendingAction && pendingRebind_.bindingIndex < pendingAction->bindings.size()
         ? pendingAction->bindings[pendingRebind_.bindingIndex]
         : InputBinding{};
      bool hasCapture = false;
      // 押下中ではなくトリガーだけを拾い、再バインド開始に使った入力を誤採用しない。
      for (const auto& [name, key] : kKnownKeys) {
         (void)name;
         if (backend_->IsKeyTriggered(key)) {
            switch (pendingRebind_.part) {
               case RebindPart::Negative: captured.negativeKey = key; break;
               case RebindPart::Positive: captured.positiveKey = key; break;
               case RebindPart::Left: captured.leftKey = key; break;
               case RebindPart::Right: captured.rightKey = key; break;
               case RebindPart::Up: captured.upKey = key; break;
               case RebindPart::Down: captured.downKey = key; break;
               case RebindPart::Whole:
               default:
                  captured = {};
                  captured.type = InputBindingType::Key;
                  captured.key = key;
                  break;
            }
            hasCapture = true;
            break;
         }
      }
      // 複合軸の一方向には同種のキーだけを許可し、バインド全体の置換時だけ
      // ゲームパッドボタンへ種別変更できるようにする。
      if (!hasCapture && pendingRebind_.part == RebindPart::Whole) {
         for (const auto& [name, button] : kKnownGamePadButtons) {
            (void)name;
            if (backend_->IsGamePadButtonTriggered(button, 0)) {
               captured.type = InputBindingType::GamePadButton;
               captured.gamePadButton = button;
               hasCapture = true;
               break;
            }
         }
      }
      if (hasCapture) {
         // まず非破壊モードで適用し、競合があった場合だけユーザー確認へ進む。
         if (ApplyBinding(pendingRebind_.map, pendingRebind_.actionId, pendingRebind_.bindingIndex, captured, false)) {
            pendingRebind_.active = false;
         } else {
            pendingConflictBinding_ = captured;
            hasPendingConflict_ = true;
            ImGui::OpenPopup("Input binding conflict");
         }
      }
      ImGui::SameLine();
      if (ImGui::SmallButton("Cancel")) { pendingRebind_.active = false; }
   }

   if (ImGui::BeginPopupModal("Input binding conflict", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::TextUnformatted("This physical input is already assigned.");
      if (ImGui::Button("Replace") && hasPendingConflict_) {
         ApplyBinding(
            pendingRebind_.map,
            pendingRebind_.actionId,
            pendingRebind_.bindingIndex,
            pendingConflictBinding_,
            true);
         pendingRebind_.active = false;
         hasPendingConflict_ = false;
         ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button("Cancel")) {
         hasPendingConflict_ = false;
         ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
   }

   if (ImGui::Button("Save Bindings")) {
      if (!SaveOverrides()) { Logger::Warning("Failed to save input bindings"); }
   }
   ImGui::SameLine();
   if (ImGui::Button("Reset To Defaults")) { ResetToDefaults(); }
}
#endif

} // namespace GameEngine
