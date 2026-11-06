#include "GameEngine/Input/InputBinding.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <type_traits>
#include <utility>

namespace GameEngine {
namespace {
using json = nlohmann::json;

const std::pair<const char*, KeyCode> kKnownKeys[] = {{"A", KeyCode::A}, {"B", KeyCode::B}, {"C", KeyCode::C},
   {"D", KeyCode::D}, {"E", KeyCode::E}, {"F", KeyCode::F}, {"G", KeyCode::G}, {"H", KeyCode::H}, {"I", KeyCode::I},
   {"J", KeyCode::J}, {"K", KeyCode::K}, {"L", KeyCode::L}, {"M", KeyCode::M}, {"N", KeyCode::N}, {"O", KeyCode::O},
   {"P", KeyCode::P}, {"Q", KeyCode::Q}, {"R", KeyCode::R}, {"S", KeyCode::S}, {"T", KeyCode::T}, {"U", KeyCode::U},
   {"V", KeyCode::V}, {"W", KeyCode::W}, {"X", KeyCode::X}, {"Y", KeyCode::Y}, {"Z", KeyCode::Z},
   {"Num0", KeyCode::Num0}, {"Num1", KeyCode::Num1}, {"Num2", KeyCode::Num2}, {"Num3", KeyCode::Num3},
   {"Num4", KeyCode::Num4}, {"Num5", KeyCode::Num5}, {"Num6", KeyCode::Num6}, {"Num7", KeyCode::Num7},
   {"Num8", KeyCode::Num8}, {"Num9", KeyCode::Num9},

   // F1はデバッグカメラ専用ショートカットのため再割り当て候補から除外する。
   {"F2", KeyCode::F2}, {"F3", KeyCode::F3}, {"F4", KeyCode::F4}, {"F5", KeyCode::F5}, {"F6", KeyCode::F6},
   {"F7", KeyCode::F7}, {"F8", KeyCode::F8}, {"F9", KeyCode::F9}, {"F10", KeyCode::F10}, {"F11", KeyCode::F11},
   {"F12", KeyCode::F12}, {"Escape", KeyCode::Escape}, {"Tab", KeyCode::Tab}, {"CapsLock", KeyCode::CapsLock},
   {"LeftShift", KeyCode::LeftShift}, {"RightShift", KeyCode::RightShift}, {"LeftCtrl", KeyCode::LeftCtrl},
   {"RightCtrl", KeyCode::RightCtrl}, {"LeftAlt", KeyCode::LeftAlt}, {"RightAlt", KeyCode::RightAlt},
   {"Space", KeyCode::Space}, {"Enter", KeyCode::Enter}, {"Backspace", KeyCode::Backspace}, {"Delete", KeyCode::Delete},
   {"Insert", KeyCode::Insert}, {"Home", KeyCode::Home}, {"End", KeyCode::End}, {"PageUp", KeyCode::PageUp},
   {"PageDown", KeyCode::PageDown}, {"Up", KeyCode::Up}, {"Down", KeyCode::Down}, {"Left", KeyCode::Left},
   {"Right", KeyCode::Right}, {"Numpad0", KeyCode::Numpad0}, {"Numpad1", KeyCode::Numpad1},
   {"Numpad2", KeyCode::Numpad2}, {"Numpad3", KeyCode::Numpad3}, {"Numpad4", KeyCode::Numpad4},
   {"Numpad5", KeyCode::Numpad5}, {"Numpad6", KeyCode::Numpad6}, {"Numpad7", KeyCode::Numpad7},
   {"Numpad8", KeyCode::Numpad8}, {"Numpad9", KeyCode::Numpad9}, {"NumpadEnter", KeyCode::NumpadEnter},
   {"NumpadPlus", KeyCode::NumpadPlus}, {"NumpadMinus", KeyCode::NumpadMinus},
   {"NumpadMultiply", KeyCode::NumpadMultiply}, {"NumpadDivide", KeyCode::NumpadDivide},
   {"NumpadDecimal", KeyCode::NumpadDecimal}, {"Minus", KeyCode::Minus}, {"Equals", KeyCode::Equals},
   {"LeftBracket", KeyCode::LeftBracket}, {"RightBracket", KeyCode::RightBracket}, {"Semicolon", KeyCode::Semicolon},
   {"Apostrophe", KeyCode::Apostrophe}, {"Grave", KeyCode::Grave}, {"Backslash", KeyCode::Backslash},
   {"Comma", KeyCode::Comma}, {"Period", KeyCode::Period}, {"Slash", KeyCode::Slash}};

const std::pair<const char*, GamePadButton> kKnownGamePadButtons[] = {{"DPadUp", GamePadButton::DPadUp},
   {"DPadDown", GamePadButton::DPadDown}, {"DPadLeft", GamePadButton::DPadLeft},
   {"DPadRight", GamePadButton::DPadRight}, {"Start", GamePadButton::Start}, {"Back", GamePadButton::Back},
   {"LeftThumb", GamePadButton::LeftThumb}, {"RightThumb", GamePadButton::RightThumb}, {"A", GamePadButton::A},
   {"B", GamePadButton::B}, {"X", GamePadButton::X}, {"Y", GamePadButton::Y},
   {"LeftShoulder", GamePadButton::LeftShoulder}, {"RightShoulder", GamePadButton::RightShoulder}};

const char* ToString(InputBindingAxis axis) {
   switch (axis) {
      case InputBindingAxis::Both:
         return "Both";
      case InputBindingAxis::X:
         return "X";
      case InputBindingAxis::Y:
         return "Y";
   }

   return "Both";
}

bool TryParseBindingAxis(const std::string& text, InputBindingAxis& axis) {
   if (text == "Both") {
      axis = InputBindingAxis::Both;
      return true;
   }

   if (text == "X") {
      axis = InputBindingAxis::X;
      return true;
   }

   if (text == "Y") {
      axis = InputBindingAxis::Y;
      return true;
   }

   return false;
}

const char* ToString(KeyCode key) {
   for (const auto& [name, value] : kKnownKeys) {
      if (value == key) {
         return name;
      }
   }

   return "Space";
}

bool TryParseKey(const std::string& text, KeyCode& key) {
   for (const auto& [name, value] : kKnownKeys) {
      if (text == name) {
         key = value;
         return true;
      }
   }

   return false;
}

const char* ToString(GamePadButton button) {
   for (const auto& [name, value] : kKnownGamePadButtons) {
      if (value == button) {
         return name;
      }
   }

   return "A";
}

bool TryParseGamePadButton(const std::string& text, GamePadButton& button) {
   for (const auto& [name, value] : kKnownGamePadButtons) {
      if (text == name) {
         button = value;
         return true;
      }
   }

   return false;
}

bool IsKnownControl(const InputPhysicalControl& control) {
   if (control.type == InputPhysicalControl::Type::Key) {
      return std::any_of(std::begin(kKnownKeys), std::end(kKnownKeys),
         [&control](const auto& entry) { return static_cast<std::uint32_t>(entry.second) == control.value; });
   }

   if (control.type == InputPhysicalControl::Type::GamePadButton) {
      return std::any_of(std::begin(kKnownGamePadButtons), std::end(kKnownGamePadButtons),
         [&control](const auto& entry) { return static_cast<std::uint32_t>(entry.second) == control.value; });
   }

   return control.value == 0 && (control.type == InputPhysicalControl::Type::LeftStick ||
                                   control.type == InputPhysicalControl::Type::RightStick);
}

auto KeyFields(KeyInputBinding& data) {
   return std::array{std::pair{"key", &data.key}};
}

auto KeyFields(KeyAxis1DInputBinding& data) {
   return std::array{std::pair{"negative", &data.negative}, std::pair{"positive", &data.positive}};
}

auto KeyFields(KeyAxis2DInputBinding& data) {
   return std::array{std::pair{"left", &data.left}, std::pair{"right", &data.right}, std::pair{"up", &data.up},
      std::pair{"down", &data.down}};
}

template <class Payload> struct KeyboardPolicy {
   static json Write(Payload data) {
      json result = json::object();

      for (const auto& [name, key] : KeyFields(data)) {
         result[name] = ToString(*key);
      }

      return result;
   }

   static bool Read(const json& source, Payload& data) {
      for (const auto& [name, key] : KeyFields(data)) {
         if (!source.contains(name) || !source.at(name).is_string() ||
             !TryParseKey(source.at(name).get<std::string>(), *key)) {
            return false;
         }
      }

      return true;
   }

   static std::vector<InputPhysicalControl> Controls(Payload data) {
      std::vector<InputPhysicalControl> result;

      for (const auto& [name, key] : KeyFields(data)) {
         (void)name;
         result.push_back({InputPhysicalControl::Type::Key, static_cast<std::uint32_t>(*key)});
      }

      return result;
   }

   static bool SetControl(Payload& data, size_t index, const InputPhysicalControl& control) {
      const auto fields = KeyFields(data);

      if (control.type != InputPhysicalControl::Type::Key || index >= fields.size()) {
         return false;
      }

      *fields[index].second = static_cast<KeyCode>(control.value);
      return true;
   }

   static std::vector<InputBindingEditPart> EditParts(Payload data) {
      const auto fields = KeyFields(data);

      if (fields.size() == 1) {
         return {{"Rebind", std::nullopt}};
      }

      std::vector<InputBindingEditPart> result;

      for (size_t index = 0; index < fields.size(); ++index) {
         result.push_back({fields[index].first, index});
      }

      return result;
   }

   static bool Valid(Payload data) {
      const auto controls = Controls(data);
      return std::all_of(controls.begin(), controls.end(), IsKnownControl);
   }
};

float KeyValue(IInputBackend& backend, KeyCode key, std::uint32_t playerSlot) {
   return playerSlot == 0 && backend.IsKeyPressed(key) ? 1.0f : 0.0f;
}

template <class Payload> struct BindingPolicy;

template <> struct BindingPolicy<KeyInputBinding> : KeyboardPolicy<KeyInputBinding> {
   static constexpr const char* kTypeName = "Key";

   static Vector2 Evaluate(const KeyInputBinding& data, IInputBackend& backend, std::uint32_t playerSlot, float) {
      return {KeyValue(backend, data.key, playerSlot), 0.0f};
   }

   static std::string Describe(const KeyInputBinding& data) { return std::string("Key ") + ToString(data.key); }
};

template <> struct BindingPolicy<KeyAxis1DInputBinding> : KeyboardPolicy<KeyAxis1DInputBinding> {
   static constexpr const char* kTypeName = "KeyAxis1D";

   static Vector2 Evaluate(const KeyAxis1DInputBinding& data, IInputBackend& backend, std::uint32_t playerSlot, float) {
      return {KeyValue(backend, data.positive, playerSlot) - KeyValue(backend, data.negative, playerSlot), 0.0f};
   }

   static std::string Describe(const KeyAxis1DInputBinding& data) {
      return std::string(ToString(data.negative)) + " / " + ToString(data.positive);
   }
};

template <> struct BindingPolicy<KeyAxis2DInputBinding> : KeyboardPolicy<KeyAxis2DInputBinding> {
   static constexpr const char* kTypeName = "KeyAxis2D";

   static Vector2 Evaluate(const KeyAxis2DInputBinding& data, IInputBackend& backend, std::uint32_t playerSlot, float) {
      return {KeyValue(backend, data.right, playerSlot) - KeyValue(backend, data.left, playerSlot),
         KeyValue(backend, data.up, playerSlot) - KeyValue(backend, data.down, playerSlot)};
   }

   static std::string Describe(const KeyAxis2DInputBinding& data) {
      return std::string(ToString(data.left)) + " / " + ToString(data.right) + " / " + ToString(data.up) + " / " +
             ToString(data.down);
   }
};

template <> struct BindingPolicy<GamePadButtonInputBinding> {
   static constexpr const char* kTypeName = "GamePadButton";

   static json Write(const GamePadButtonInputBinding& data) { return {{"button", ToString(data.button)}}; }

   static bool Read(const json& source, GamePadButtonInputBinding& data) {
      return source.contains("button") && source.at("button").is_string() &&
             TryParseGamePadButton(source.at("button").get<std::string>(), data.button);
   }

   static Vector2 Evaluate(
      const GamePadButtonInputBinding& data, IInputBackend& backend, std::uint32_t playerSlot, float) {
      return {backend.IsGamePadButtonPressed(data.button, playerSlot) ? 1.0f : 0.0f, 0.0f};
   }

   static std::string Describe(const GamePadButtonInputBinding& data) {
      return std::string("GamePad ") + ToString(data.button);
   }

   static std::vector<InputPhysicalControl> Controls(const GamePadButtonInputBinding& data) {
      return {{InputPhysicalControl::Type::GamePadButton, static_cast<std::uint32_t>(data.button)}};
   }

   static bool SetControl(GamePadButtonInputBinding& data, size_t index, const InputPhysicalControl& control) {
      if (index != 0 || control.type != InputPhysicalControl::Type::GamePadButton) {
         return false;
      }

      data.button = static_cast<GamePadButton>(control.value);
      return true;
   }

   static std::vector<InputBindingEditPart> EditParts(const GamePadButtonInputBinding&) {
      return {{"Rebind", std::nullopt}};
   }

   static bool Valid(const GamePadButtonInputBinding& data) { return IsKnownControl(Controls(data).front()); }
};

template <class Payload, bool IsLeft> struct StickPolicy {
   static json Write(const Payload& data) { return {{"axis", ToString(data.axis)}}; }

   static bool Read(const json& source, Payload& data) {
      return source.contains("axis") && source.at("axis").is_string() &&
             TryParseBindingAxis(source.at("axis").get<std::string>(), data.axis);
   }

   static Vector2 Evaluate(const Payload& data, IInputBackend& backend, std::uint32_t playerSlot, float deadZone) {
      Vector2 value = IsLeft ? backend.GetLeftStick(playerSlot, deadZone) : backend.GetRightStick(playerSlot, deadZone);

      // 一次元アクションはX成分を読むため、Y指定の入力もXへ詰め替える。
      if (data.axis == InputBindingAxis::X) {
         value.y = 0.0f;
      } else if (data.axis == InputBindingAxis::Y) {
         value.x = value.y;
         value.y = 0.0f;
      }

      return value;
   }

   static std::string Describe(const Payload& data) {
      return std::string(IsLeft ? "Left Stick " : "Right Stick ") + ToString(data.axis);
   }

   static std::vector<InputPhysicalControl> Controls(const Payload&) {
      return {{IsLeft ? InputPhysicalControl::Type::LeftStick : InputPhysicalControl::Type::RightStick, 0}};
   }

   static bool SetControl(Payload&, size_t, const InputPhysicalControl&) { return false; }

   static std::vector<InputBindingEditPart> EditParts(const Payload&) { return {}; }

   static bool Valid(const Payload& data) {
      return data.axis == InputBindingAxis::Both || data.axis == InputBindingAxis::X ||
             data.axis == InputBindingAxis::Y;
   }
};

template <> struct BindingPolicy<LeftStickInputBinding> : StickPolicy<LeftStickInputBinding, true> {
   static constexpr const char* kTypeName = "LeftStick";
};

template <> struct BindingPolicy<RightStickInputBinding> : StickPolicy<RightStickInputBinding, false> {
   static constexpr const char* kTypeName = "RightStick";
};

template <size_t... Indices>
std::optional<InputBinding> ReadBinding(
   const json& source, const std::string& type, float scale, std::index_sequence<Indices...>) {
   std::optional<InputBinding> result;
   const auto tryPayload = [&]<class Payload>() {
      if (type != BindingPolicy<Payload>::kTypeName) return;

      Payload data;

      if (BindingPolicy<Payload>::Read(source, data)) {
         result = InputBinding{std::move(data), scale};
      }
   };
   (tryPayload.template operator()<std::variant_alternative_t<Indices, InputBinding::Source>>(), ...);
   return result;
}

} // namespace

Vector2 InputBinding::Evaluate(IInputBackend& backend, std::uint32_t playerSlot, float deadZone) const {
   if (!std::isfinite(scale)) {
      return {};
   }

   Vector2 value = std::visit(
      [&](const auto& data) {
         return BindingPolicy<std::decay_t<decltype(data)>>::Evaluate(data, backend, playerSlot, deadZone);
      },
      source);
   value.x *= scale;
   value.y *= scale;
   return value;
}

nlohmann::json InputBinding::Serialize() const {
   json result = std::visit(
      [](const auto& data) {
         using Policy = BindingPolicy<std::decay_t<decltype(data)>>;
         json encoded = Policy::Write(data);
         encoded["type"] = Policy::kTypeName;
         return encoded;
      },
      source);
   result["scale"] = scale;
   return result;
}

std::optional<InputBinding> InputBinding::Deserialize(const nlohmann::json& data) {
   try {
      if (!data.is_object() || !data.contains("type") || !data.at("type").is_string() ||
          (data.contains("scale") && !data.at("scale").is_number())) {
         return std::nullopt;
      }

      const float scale = data.value("scale", 1.0f);

      if (!std::isfinite(scale)) {
         return std::nullopt;
      }

      return ReadBinding(
         data, data.at("type").get<std::string>(), scale, std::make_index_sequence<std::variant_size_v<Source>>{});
   } catch (const json::exception&) {
      return std::nullopt;
   }
}

std::string InputBinding::Describe() const {
   return std::visit(
      [](const auto& data) { return BindingPolicy<std::decay_t<decltype(data)>>::Describe(data); }, source);
}

bool InputBinding::IsValid() const {
   return std::isfinite(scale) &&
          std::visit([](const auto& data) { return BindingPolicy<std::decay_t<decltype(data)>>::Valid(data); }, source);
}

std::vector<InputPhysicalControl> InputBinding::GetPhysicalControls() const {
   return std::visit(
      [](const auto& data) { return BindingPolicy<std::decay_t<decltype(data)>>::Controls(data); }, source);
}

std::vector<InputBindingEditPart> InputBinding::GetEditParts() const {
   return std::visit(
      [](const auto& data) { return BindingPolicy<std::decay_t<decltype(data)>>::EditParts(data); }, source);
}

bool InputBinding::SetPhysicalControl(size_t index, const InputPhysicalControl& control) {
   if (!IsKnownControl(control)) {
      return false;
   }

   return std::visit(
      [&](auto& data) { return BindingPolicy<std::decay_t<decltype(data)>>::SetControl(data, index, control); },
      source);
}

bool InputBinding::ReplacePhysicalControl(const InputPhysicalControl& target, const InputPhysicalControl& replacement) {
   if (target.type != replacement.type) {
      return false;
   }

   const auto controls = GetPhysicalControls();
   bool replaced = false;

   for (size_t index = 0; index < controls.size(); ++index) {
      if (controls[index] == target) {
         replaced = SetPhysicalControl(index, replacement) || replaced;
      }
   }

   return replaced;
}

bool InputBinding::SharesPhysicalControl(const InputBinding& other) const {
   const auto controls = GetPhysicalControls();
   const auto otherControls = other.GetPhysicalControls();
   return std::any_of(controls.begin(), controls.end(), [&otherControls](const InputPhysicalControl& control) {
      return std::find(otherControls.begin(), otherControls.end(), control) != otherControls.end();
   });
}

bool InputBinding::ContainsPhysicalControl(const InputPhysicalControl& control) const {
   const auto controls = GetPhysicalControls();
   return std::find(controls.begin(), controls.end(), control) != controls.end();
}

std::optional<InputBinding> InputBinding::Rebind(
   const InputPhysicalControl& control, std::optional<size_t> controlIndex) const {
   InputBinding result = *this;

   if (controlIndex) {
      if (control.type != InputPhysicalControl::Type::Key || !result.SetPhysicalControl(*controlIndex, control)) {
         return std::nullopt;
      }
   } else if (control.type == InputPhysicalControl::Type::Key && IsKnownControl(control)) {
      result.source = KeyInputBinding{static_cast<KeyCode>(control.value)};
   } else if (control.type == InputPhysicalControl::Type::GamePadButton && IsKnownControl(control)) {
      result.source = GamePadButtonInputBinding{static_cast<GamePadButton>(control.value)};
   } else {
      return std::nullopt;
   }

   return result.IsValid() ? std::optional{result} : std::nullopt;
}

std::optional<InputPhysicalControl> CaptureRebindControl(IInputBackend& backend, bool allowGamePad) {
   // 押下中ではなくトリガーだけを拾い、待受開始前から押されている入力を誤採用しない。
   for (const auto& [name, key] : kKnownKeys) {
      (void)name;

      if (backend.IsKeyTriggered(key)) {
         return InputPhysicalControl{InputPhysicalControl::Type::Key, static_cast<std::uint32_t>(key)};
      }
   }

   if (allowGamePad) {
      for (const auto& [name, button] : kKnownGamePadButtons) {
         (void)name;

         if (backend.IsGamePadButtonTriggered(button, 0)) {
            return InputPhysicalControl{InputPhysicalControl::Type::GamePadButton, static_cast<std::uint32_t>(button)};
         }
      }
   }

   return std::nullopt;
}

} // namespace GameEngine
