#include "GameEngine/Input/InputActionService.h"
#include "GameEngine/Utility/JsonFile.h"
#include "GameEngine/Utility/Logger.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <utility>

#ifdef USE_IMGUI
#include "imgui.h"
#endif

namespace GameEngine {
namespace {
using json = nlohmann::json;

constexpr std::int32_t kInputActionFormatVersion = 1;
constexpr std::int32_t kJsonIndentSize = 2;
constexpr float kMaximumStickDeadZone = 0.95f;
constexpr float kHeldThreshold = 0.0001f;
const InputActionState kEmptyActionState{};

const char* ToString(InputActionType type) {
   switch (type) {
      case InputActionType::Button:
         return "Button";
      case InputActionType::Axis1D:
         return "Axis1D";
      case InputActionType::Axis2D:
         return "Axis2D";
   }

   return "Button";
}

bool TryParseActionType(const std::string& text, InputActionType& type) {
   if (text == "Button") {
      type = InputActionType::Button;
      return true;
   }

   if (text == "Axis1D") {
      type = InputActionType::Axis1D;
      return true;
   }

   if (text == "Axis2D") {
      type = InputActionType::Axis2D;
      return true;
   }

   return false;
}

} // namespace

bool InputActionService::FindSingleChangedControl(const InputBinding& previous, const InputBinding& replacement,
   InputPhysicalControl& previousControl, InputPhysicalControl& replacementControl) {
   // 競合時に安全に入れ替えられるのは、同じ形のバインドで一つの物理入力だけを
   // 変更した場合に限る。複数変更を推測で交換すると別方向の割り当てまで壊れてしまう。
   if (previous.source.index() != replacement.source.index()) {
      return false;
   }

   const auto previousControls = previous.GetPhysicalControls();
   const auto replacementControls = replacement.GetPhysicalControls();

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

void InputActionService::Initialize(
   IInputBackend& backend, const std::filesystem::path& defaultPath, const std::filesystem::path& overridePath) {
   rebindSession_ = {};
   frameNumber_ = 0;
   backend_ = &backend;
   defaultPath_ = defaultPath;
   overridePath_ = overridePath;

   // 組み込み定義を土台に外部定義を重ねる。ファイルが欠けても最低限の操作を失わず、
   // 正常に読めた定義だけを「リセット先」として確定してからユーザー設定を適用する。
   BuildFallbackDefaults();

   if (!LoadDefinitions(defaultPath_, true)) {
      Logger::Warning("Input actions default file is missing or invalid; built-in defaults are active: " +
                      defaultPath_.generic_string());
   }

   defaults_ = actions_;
   LoadOverrides();
}

void InputActionService::BuildFallbackDefaults() {
   // 起動不能を避けるための最小構成。外部定義が壊れていても、同じ意味の操作を維持する。
   actions_.clear();
   const auto addAction = [this](const char* map, const char* id, InputActionType type,
                             std::initializer_list<InputBinding> bindings) {
      ActionDefinition action;
      action.map = map;
      action.id = id;
      action.type = type;
      action.bindings.assign(bindings.begin(), bindings.end());
      actions_[MakeKey(map, id)] = std::move(action);
   };

   addAction("Gameplay", "Vehicle.Steer", InputActionType::Axis1D,
      {{KeyAxis1DInputBinding{KeyCode::A, KeyCode::D}}, {LeftStickInputBinding{InputBindingAxis::X}}});
   addAction("Gameplay", "Vehicle.Pitch", InputActionType::Axis1D,
      {{KeyAxis1DInputBinding{KeyCode::S, KeyCode::W}}, {LeftStickInputBinding{InputBindingAxis::Y}}});

   // 左右ショルダーは逆符号で加算し、同時押しなら相殺する。
   addAction("Gameplay", "Vehicle.Roll", InputActionType::Axis1D,
      {{KeyAxis1DInputBinding{KeyCode::Q, KeyCode::E}}, {GamePadButtonInputBinding{GamePadButton::LeftShoulder}, -1.0f},
         {GamePadButtonInputBinding{GamePadButton::RightShoulder}, 1.0f}});
   addAction("Gameplay", "Vehicle.Jump", InputActionType::Button,
      {{KeyInputBinding{KeyCode::Space}}, {GamePadButtonInputBinding{GamePadButton::A}}});
   addAction("Gameplay", "Vehicle.Drift", InputActionType::Button,
      {{KeyInputBinding{KeyCode::Q}}, {GamePadButtonInputBinding{GamePadButton::LeftShoulder}}});
   addAction("Gameplay", "Camera.Look", InputActionType::Axis2D,
      {{KeyAxis2DInputBinding{}}, {RightStickInputBinding{InputBindingAxis::Both}}});
   addAction("Gameplay", "Camera.Next", InputActionType::Button, {{KeyInputBinding{KeyCode::Tab}}});
   addAction("UI", "UI.Navigate", InputActionType::Axis1D,
      {{KeyAxis1DInputBinding{KeyCode::W, KeyCode::S}}, {GamePadButtonInputBinding{GamePadButton::DPadUp}, -1.0f},
         {GamePadButtonInputBinding{GamePadButton::DPadDown}}, {LeftStickInputBinding{InputBindingAxis::Y}, -1.0f}});
   addAction("UI", "UI.Confirm", InputActionType::Button,
      {{KeyInputBinding{KeyCode::Space}}, {GamePadButtonInputBinding{GamePadButton::A}}});
   addAction("UI", "UI.Cancel", InputActionType::Button,
      {{KeyInputBinding{KeyCode::Escape}}, {GamePadButtonInputBinding{GamePadButton::B}},
         {GamePadButtonInputBinding{GamePadButton::Start}}});
}

bool InputActionService::LoadDefinitions(const std::filesystem::path& path, bool replaceAll) {
   std::ifstream stream(path);

   if (!stream) {
      return false;
   }

   json root;
   try {
      stream >> root;
   } catch (const json::exception&) {
      return false;
   }

   if (!root.contains("maps") || !root["maps"].is_object()) {
      return false;
   }

   // 読み込み途中で不正項目が見つかっても現在の設定を壊さないよう、作業用コピーへ適用する。
   // replaceAll は未知アクションの追加可否を表し、override 読み込み時は既知項目だけを許可する。
   auto loadedActions = actions_;
   size_t appliedCount = 0;

   for (const auto& [mapName, mapData] : root["maps"].items()) {
      if (!mapData.is_object()) {
         continue;
      }

      for (const auto& [actionId, actionData] : mapData.items()) {
         if (!actionData.is_object()) {
            continue;
         }

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
             !TryParseActionType(actionData["type"].get<std::string>(), type) || !actionData.contains("bindings") ||
             !actionData["bindings"].is_array()) {
            Logger::Warning("Invalid input action; retaining fallback: " + mapName + "/" + actionId);
            continue;
         }

         // 配列全体を検証してから置換する。途中まで有効な配列は操作体系を予測不能にするため採用しない。
         std::vector<InputBinding> bindings;
         bool valid = true;

         for (const json& bindingData : actionData["bindings"]) {
            auto binding = InputBinding::Deserialize(bindingData);

            if (!binding) {
               valid = false;
               break;
            }

            bindings.push_back(std::move(*binding));
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
         const float configuredDeadZone =
            actionData.contains("deadZone") ? actionData.at("deadZone").get<float>() : action.deadZone;

         // 1.0ではスティックが決して反応しなくなるため、上限にはわずかな余裕を残す。
         action.deadZone = std::isfinite(configuredDeadZone)
                              ? std::clamp(configuredDeadZone, 0.0f, kMaximumStickDeadZone)
                              : action.deadZone;
         action.bindings = std::move(bindings);
         loadedActions[key] = std::move(action);
         ++appliedCount;
      }
   }

   if (replaceAll && appliedCount == 0) {
      return false;
   }

   actions_ = std::move(loadedActions);

   if (rebindSession_.IsActive()) {
      rebindSession_.Cancel();
   }

   return true;
}

void InputActionService::Update() {
   if (!backend_) return;

   ++frameNumber_;

   // 全アクションをプレイヤー単位で評価し、このフレームのスナップショットを完成させる。
   // 利用側が同じフレームに何度問い合わせても triggered/released が変化しない設計である。
   for (auto& [key, action] : actions_) {
      (void)key;

      for (uint32_t playerSlot = 0; playerSlot < kMaxPlayers; ++playerSlot) {
         Vector2 value{};

         for (const InputBinding& binding : action.bindings) {
            const Vector2 bindingValue = binding.Evaluate(*backend_, playerSlot, action.deadZone);

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
         state.held = std::abs(value.x) > kHeldThreshold || std::abs(value.y) > kHeldThreshold;
         state.triggered = state.held && !wasHeld;
         state.released = !state.held && wasHeld;
         state.frameNumber = frameNumber_;
      }
   }

   UpdateRebind();
}

const InputActionState& InputActionService::GetActionState(
   const std::string& actionMap, const std::string& actionId, uint32_t playerSlot) const {
   const ActionDefinition* action = FindAction(actionMap, actionId);

   if ((gameplayInputBlocked_ && actionMap == "Gameplay") || !action || playerSlot >= kMaxPlayers) {
      return kEmptyActionState;
   }

   return action->states[playerSlot];
}

bool InputActionService::HasHeldActions() const {
   for (const auto& [key, definition] : actions_) {
      for (const auto& state : definition.states) {
         if (state.held) {
            return true;
         }
      }
   }
   return false;
}

bool InputActionService::BeginRebind(
   const std::string& actionMap, const std::string& actionId, size_t bindingIndex, std::optional<size_t> controlIndex) {
   const auto* action = FindAction(actionMap, actionId);

   if (!backend_ || !action || bindingIndex >= action->bindings.size()) {
      return false;
   }

   return rebindSession_.Begin(
      {actionMap, actionId, bindingIndex, controlIndex}, action->bindings[bindingIndex], frameNumber_);
}

bool InputActionService::HasRebindTarget() const {
   const auto& target = rebindSession_.GetTarget();

   if (!target) {
      return false;
   }

   const auto* action = FindAction(target->map, target->actionId);
   return action && target->bindingIndex < action->bindings.size();
}

bool InputActionService::ApplyRebindCandidate(const InputBinding& binding, bool replaceConflict) {
   const auto& target = rebindSession_.GetTarget();
   return target && ApplyBinding(target->map, target->actionId, target->bindingIndex, binding, replaceConflict);
}

void InputActionService::UpdateRebind() {
   if (!rebindSession_.IsActive()) return;

   if (!HasRebindTarget()) {
      rebindSession_.Cancel();
      return;
   }

   rebindSession_.Update(*backend_, frameNumber_, [this](const InputBinding& binding, bool replaceConflict) {
      return ApplyRebindCandidate(binding, replaceConflict);
   });
}

bool InputActionService::ConfirmRebindConflict() {
   if (!HasRebindTarget()) {
      rebindSession_.Cancel();
      return false;
   }

   return rebindSession_.ConfirmConflict([this](const InputBinding& binding, bool replaceConflict) {
      return ApplyRebindCandidate(binding, replaceConflict);
   });
}

void InputActionService::RejectRebindConflict() {
   rebindSession_.RejectConflict(frameNumber_);
}

void InputActionService::CancelRebind() {
   rebindSession_.Cancel();
}

bool InputActionService::ApplyBinding(const std::string& actionMap, const std::string& actionId, size_t bindingIndex,
   const InputBinding& binding, bool replaceConflict) {
   ActionDefinition* target = FindAction(actionMap, actionId);

   if (!target || bindingIndex >= target->bindings.size() || !binding.IsValid()) {
      return false;
   }

   const InputBinding previousTargetBinding = target->bindings[bindingIndex];

   // 一方向だけを再割り当てた複合軸では、競合先の同じ物理入力と旧入力を交換する。
   // これにより「左をDへ変更」した際、既存の右=Dが未割り当てになるのを防ぐ。
   InputPhysicalControl previousControl{};
   InputPhysicalControl replacementControl{};
   const bool changesSingleControl =
      FindSingleChangedControl(previousTargetBinding, binding, previousControl, replacementControl);
   InputBinding resolvedTargetBinding = binding;

   // 対象バインド自身は下の競合走査から除外されるため、変更先が複合軸の
   // 未変更部分ですでに使われている場合はここで検出し、承認時は両方向を交換する。
   if (changesSingleControl) {
      const auto previousTargetControls = previousTargetBinding.GetPhysicalControls();
      const auto selfConflict =
         std::find(previousTargetControls.begin(), previousTargetControls.end(), replacementControl);

      if (selfConflict != previousTargetControls.end()) {
         if (!replaceConflict) {
            return false;
         }

         const size_t selfConflictIndex =
            static_cast<size_t>(std::distance(previousTargetControls.begin(), selfConflict));

         if (!resolvedTargetBinding.SetPhysicalControl(selfConflictIndex, previousControl)) {
            return false;
         }
      }
   }

   // アクションマップは利用コンテキストの境界であり、UIとGameplayのように
   // 同じ物理入力を共有できるため、競合解決は同一マップ内だけに限定する。
   for (auto& [key, action] : actions_) {
      (void)key;

      if (action.map != target->map) {
         continue;
      }

      for (InputBinding& existing : action.bindings) {
         const bool isTarget = &action == target && &existing == &target->bindings[bindingIndex];

         // 複合軸の片側変更時は、変更していない側の既存共有を競合としない。
         const bool conflictsWithReplacement = changesSingleControl
                                                  ? existing.ContainsPhysicalControl(replacementControl)
                                                  : existing.SharesPhysicalControl(binding);

         if (!isTarget && conflictsWithReplacement) {
            if (!replaceConflict) {
               return false;
            }

            if (!changesSingleControl || !existing.ReplacePhysicalControl(replacementControl, previousControl)) {
               // 安全な局所交換ができない場合は、競合側へ対象の旧バインド全体を移す。
               existing = previousTargetBinding;
            }
         }
      }
   }

   target->bindings[bindingIndex] = resolvedTargetBinding;
   return true;
}

bool InputActionService::LoadOverrides() {
   if (!std::filesystem::exists(overridePath_)) {
      Logger::Info("Input override file is missing; defaults are active: " + overridePath_.generic_string());
      return false;
   }

   if (!LoadDefinitions(overridePath_, false)) {
      Logger::Warning(
         "Input override file is invalid; defaults are active per action: " + overridePath_.generic_string());
      return false;
   }

   return true;
}

bool InputActionService::SaveOverrides() const {
   // 現在の完全な割り当てを保存し、既定定義の変更でユーザー設定を変化させない。
   json root;
   root["version"] = kInputActionFormatVersion;

   for (const auto& [key, action] : actions_) {
      (void)key;
      json actionData;
      actionData["type"] = ToString(action.type);
      actionData["deadZone"] = action.deadZone;
      actionData["bindings"] = json::array();

      for (const auto& binding : action.bindings) {
         actionData["bindings"].push_back(binding.Serialize());
      }

      root["maps"][action.map][action.id] = std::move(actionData);
   }

   return SaveJsonFileAtomically(overridePath_, root, kJsonIndentSize);
}

void InputActionService::ResetToDefaults() {
   actions_ = defaults_;
   rebindSession_.Cancel();
}

InputActionService::ActionDefinition* InputActionService::FindAction(
   const std::string& actionMap, const std::string& actionId) {
   auto it = actions_.find(MakeKey(actionMap, actionId));
   return it != actions_.end() ? &it->second : nullptr;
}

const InputActionService::ActionDefinition* InputActionService::FindAction(
   const std::string& actionMap, const std::string& actionId) const {
   auto it = actions_.find(MakeKey(actionMap, actionId));
   return it != actions_.end() ? &it->second : nullptr;
}

std::string InputActionService::MakeKey(const std::string& actionMap, const std::string& actionId) {
   // UI表示用文字列と衝突しにくい改行を区切りに使い、map/id の組を単一キーへまとめる。
   return actionMap + '\n' + actionId;
}

#ifdef USE_IMGUI
void InputActionService::DrawImGui() {
   if (!ImGui::CollapsingHeader("Input Actions")) return;

   ImGui::TextUnformatted("Keyboard: Player 0, XInput: matching player slot");

   // 表示順とUIのIDをmap/idで固定し、同名アクションが別マップにあっても操作対象を混同しない。
   std::vector<const ActionDefinition*> sortedActions;
   sortedActions.reserve(actions_.size());

   for (const auto& [key, action] : actions_) {
      (void)key;
      sortedActions.push_back(&action);
   }

   std::sort(sortedActions.begin(), sortedActions.end(), [](const auto* left, const auto* right) {
      return MakeKey(left->map, left->id) < MakeKey(right->map, right->id);
   });

   for (const auto* action : sortedActions) {
      const auto actionKey = MakeKey(action->map, action->id);
      ImGui::PushID(actionKey.c_str());
      ImGui::Text("%s / %s (%s)", action->map.c_str(), action->id.c_str(), ToString(action->type));

      for (size_t index = 0; index < action->bindings.size(); ++index) {
         ImGui::PushID(static_cast<std::int32_t>(index));
         const auto& binding = action->bindings[index];
         ImGui::BulletText("%s", binding.Describe().c_str());

         for (const auto& part : binding.GetEditParts()) {
            ImGui::SameLine();

            if (ImGui::SmallButton(part.label)) {
               BeginRebind(action->map, action->id, index, part.controlIndex);
            }
         }

         ImGui::PopID();
      }

      ImGui::PopID();
   }

   const auto& session = GetRebindSession();

   if (session.GetState() == InputRebindState::WaitingForInput && session.GetTarget()) {
      ImGui::Separator();
      ImGui::Text("Press an input for %s", session.GetTarget()->actionId.c_str());
      ImGui::SameLine();

      if (ImGui::SmallButton("Cancel Rebind")) {
         CancelRebind();
      }
   }

   // 描画はセッションの状態を表示し、確認・取消の要求だけを送る。入力の捕捉はUpdateが担当する。
   constexpr const char* kConflictPopup = "Input binding conflict";

   if (session.GetState() == InputRebindState::WaitingForConflict && !ImGui::IsPopupOpen(kConflictPopup)) {
      ImGui::OpenPopup(kConflictPopup);
   }

   if (ImGui::BeginPopupModal(kConflictPopup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      if (session.GetState() != InputRebindState::WaitingForConflict) {
         ImGui::CloseCurrentPopup();
      } else {
         ImGui::TextUnformatted("This physical input is already assigned.");

         if (session.GetCandidate()) {
            ImGui::TextUnformatted(session.GetCandidate()->Describe().c_str());
         }

         if (ImGui::Button("Replace") && ConfirmRebindConflict()) {
            ImGui::CloseCurrentPopup();
         }

         ImGui::SameLine();

         if (ImGui::Button("Choose another input")) {
            RejectRebindConflict();
            ImGui::CloseCurrentPopup();
         }

         ImGui::SameLine();

         if (ImGui::Button("Cancel")) {
            CancelRebind();
            ImGui::CloseCurrentPopup();
         }
      }

      ImGui::EndPopup();
   }

   if (ImGui::Button("Save Bindings")) {
      if (!SaveOverrides()) {
         Logger::Warning("Failed to save input bindings");
      }
   }

   ImGui::SameLine();

   if (ImGui::Button("Reset To Defaults")) {
      ResetToDefaults();
   }
}
#endif

} // namespace GameEngine
