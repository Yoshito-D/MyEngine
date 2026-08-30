#pragma once

#include "Input.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace GameEngine {

/// @brief 入力アクションが返す値の種類
enum class InputActionType {
   Button,
   Axis1D,
   Axis2D
};

/// @brief 1フレーム分の入力アクション状態
struct InputActionState {
   Vector2 value{};
   bool held = false;
   bool triggered = false;
   bool released = false;
   uint64_t frameNumber = 0;
};

/// @brief アクションへ割り当てられる入力の種類
enum class InputBindingType {
   Key,
   GamePadButton,
   KeyAxis1D,
   KeyAxis2D,
   LeftStick,
   RightStick
};

/// @brief スティック入力からアクションへ渡す成分
enum class InputBindingAxis {
   Both,
   X,
   Y
};

/// @brief 物理入力とアクションの対応情報
struct InputBinding {
   InputBindingType type = InputBindingType::Key;
   KeyCode key = KeyCode::Space;
   GamePadButton gamePadButton = GamePadButton::A;
   KeyCode negativeKey = KeyCode::A;
   KeyCode positiveKey = KeyCode::D;
   KeyCode leftKey = KeyCode::Left;
   KeyCode rightKey = KeyCode::Right;
   KeyCode upKey = KeyCode::Up;
   KeyCode downKey = KeyCode::Down;
   InputBindingAxis axis = InputBindingAxis::Both;
   float scale = 1.0f;
};

/// @brief 物理入力をゲーム用アクションへ変換するサービス
class InputActionService final {
public:
   static constexpr uint32_t kMaxPlayers = 4;

   /// @brief 入力バックエンドと設定ファイルを指定して初期化する
   /// @param backend 低レベル入力バックエンド
   /// @param defaultPath 既定アクション設定
   /// @param overridePath ユーザー上書き設定
   void Initialize(
      IInputBackend& backend,
      const std::filesystem::path& defaultPath,
      const std::filesystem::path& overridePath);

   /// @brief 入力アクションを1フレーム進める
   void Update();

   /// @brief 指定プレイヤーのアクション状態を取得する
   /// @param actionMap アクションマップ名
   /// @param actionId 安定したアクションID
   /// @param playerSlot プレイヤースロット
   /// @return 見つからない場合はゼロ状態
   const InputActionState& GetActionState(
      const std::string& actionMap,
      const std::string& actionId,
      uint32_t playerSlot = 0) const;

   /// @brief 入力再割り当ての待受を開始する
   /// @return アクションが存在する場合はtrue
   bool BeginRebind(const std::string& actionMap, const std::string& actionId, size_t bindingIndex);

   /// @brief 指定アクションへバインドを適用する
   /// @details 物理入力の競合は同一アクションマップ内でのみ検出・置換する。
   /// @return 競合がないか置換が許可された場合はtrue
   bool ApplyBinding(
      const std::string& actionMap,
      const std::string& actionId,
      size_t bindingIndex,
      const InputBinding& binding,
      bool replaceConflict);

   /// @brief ユーザー上書き設定を読み込む
   /// @return ファイル全体が正常だった場合はtrue
   bool LoadOverrides();

   /// @brief ユーザー上書き設定を一時ファイル経由で保存する
   /// @return 保存できた場合はtrue
   bool SaveOverrides() const;

   /// @brief すべての割り当てを既定値へ戻す
   void ResetToDefaults();

#ifdef USE_IMGUI
   /// @brief 入力設定パネルを描画する
   void DrawImGui();
#endif

private:
   enum class RebindPart {
      Whole,
      Negative,
      Positive,
      Left,
      Right,
      Up,
      Down,
   };

   struct ActionDefinition {
      std::string map;
      std::string id;
      InputActionType type = InputActionType::Button;
      float deadZone = 0.24f;
      std::vector<InputBinding> bindings;
      std::array<InputActionState, kMaxPlayers> states{};
   };

   struct PendingRebind {
      std::string map;
      std::string actionId;
      size_t bindingIndex = 0;
      RebindPart part = RebindPart::Whole;
      bool active = false;
   };

   void BuildFallbackDefaults();
   bool LoadDefinitions(const std::filesystem::path& path, bool replaceAll);
   ActionDefinition* FindAction(const std::string& actionMap, const std::string& actionId);
   const ActionDefinition* FindAction(const std::string& actionMap, const std::string& actionId) const;
   bool BeginRebindPart(
      const std::string& actionMap,
      const std::string& actionId,
      size_t bindingIndex,
      RebindPart part);
   Vector2 EvaluateBinding(const InputBinding& binding, uint32_t playerSlot, float deadZone) const;
   static std::string MakeKey(const std::string& actionMap, const std::string& actionId);

   IInputBackend* backend_ = nullptr;
   std::filesystem::path defaultPath_;
   std::filesystem::path overridePath_;
   std::unordered_map<std::string, ActionDefinition> actions_;
   std::unordered_map<std::string, ActionDefinition> defaults_;
   PendingRebind pendingRebind_;
   InputBinding pendingConflictBinding_{};
   bool hasPendingConflict_ = false;
   uint64_t frameNumber_ = 0;
};

} // namespace GameEngine
