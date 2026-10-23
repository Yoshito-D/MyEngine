#pragma once

#include "GameEngine/Input/Input.h"
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
   Vector2 value{};          ///< アクションの評価済み入力値。ボタンと1次元軸はX成分を使う。
   bool held = false;        ///< 現在のフレームで入力が押下状態かどうか。
   bool triggered = false;   ///< このフレームで入力が押下へ変化したかどうか。
   bool released = false;    ///< このフレームで入力が解放へ変化したかどうか。
   uint64_t frameNumber = 0; ///< この入力状態を評価したフレーム番号。
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
   InputBindingType type = InputBindingType::Key;  ///< 処理対象の種類を区別する識別値。
   KeyCode key = KeyCode::Space;                   ///< 単一ボタンのアクションへ割り当てるキー。
   GamePadButton gamePadButton = GamePadButton::A; ///< アクションへ割り当てるゲームパッドボタン。
   KeyCode negativeKey = KeyCode::A;               ///< 1次元入力の負方向へ割り当てるキー。
   KeyCode positiveKey = KeyCode::D;               ///< 1次元入力の正方向へ割り当てるキー。
   KeyCode leftKey = KeyCode::Left;                ///< 2次元入力の左方向へ割り当てるキー。
   KeyCode rightKey = KeyCode::Right;              ///< 2次元入力の右方向へ割り当てるキー。
   KeyCode upKey = KeyCode::Up;                    ///< 2次元入力の上方向へ割り当てるキー。
   KeyCode downKey = KeyCode::Down;                ///< 2次元入力の下方向へ割り当てるキー。
   InputBindingAxis axis = InputBindingAxis::Both; ///< スティックのX・Y・両方のどれを使うかの設定。
   float scale = 1.0f;                             ///< 各軸の拡大率。
};

/// @brief 物理入力をゲーム用アクションへ変換するサービス
class InputActionService final {
public:
   static constexpr uint32_t kMaxPlayers = 4; ///< 入力状態を独立して保持するプレイヤースロット数。

   /// @brief 入力バックエンドと設定ファイルを指定して初期化する
   /// @param backend 低レベル入力バックエンド
   /// @param defaultPath 既定アクション設定
   /// @param overridePath ユーザー上書き設定
   void Initialize(
      IInputBackend& backend, const std::filesystem::path& defaultPath, const std::filesystem::path& overridePath);

   /// @brief 入力アクションを1フレーム進める
   void Update();

   /// @brief 指定プレイヤーのアクション状態を取得する
   /// @param actionMap アクションマップ名
   /// @param actionId 安定したアクションID
   /// @param playerSlot プレイヤースロット
   /// @return 見つからない場合はゼロ状態
   const InputActionState& GetActionState(
      const std::string& actionMap, const std::string& actionId, uint32_t playerSlot = 0) const;

   /// @brief 入力再割り当ての待受を開始する
   /// @return アクションが存在する場合はtrue
   bool BeginRebind(const std::string& actionMap, const std::string& actionId, size_t bindingIndex);

   /// @brief 指定アクションへバインドを適用する
   /// @details 物理入力の競合は同一アクションマップ内でのみ検出・置換する。
   /// @return 競合がないか置換が許可された場合はtrue
   bool ApplyBinding(const std::string& actionMap, const std::string& actionId, size_t bindingIndex,
      const InputBinding& binding, bool replaceConflict);

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
   /// @brief バインドの競合判定に使う物理入力の種類。
   enum class PhysicalControlType {
      Key,
      GamePadButton,
      LeftStick,
      RightStick,
   };

   /// @brief 複合バインドから取り出した、デバイス上の単一入力。
   struct PhysicalControl {
      PhysicalControlType type; ///< キー・ゲームパッドボタン・左右スティックの区別。
      uint32_t value;           ///< キーまたはボタンの識別値。スティックの場合は0。

      bool operator==(const PhysicalControl&) const = default;
   };

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
      std::string map;                                    ///< アクションをまとめる入力マップ名。
      std::string id;                                     ///< 同じ種類の要素を一意に識別するID。
      InputActionType type = InputActionType::Button;     ///< 処理対象の種類を区別する識別値。
      float deadZone = 0.24f;                             ///< スティック入力を中立とみなす範囲。
      std::vector<InputBinding> bindings;                 ///< このアクションへ割り当てた物理入力の設定一覧。
      std::array<InputActionState, kMaxPlayers> states{}; ///< プレイヤースロットごとの評価済みアクション状態。
   };

   struct PendingRebind {
      std::string map;                     ///< アクションをまとめる入力マップ名。
      std::string actionId;                ///< 入力マップ内で対象アクションを識別するID。
      size_t bindingIndex = 0;             ///< 再割り当て対象のバインド番号。
      RebindPart part = RebindPart::Whole; ///< 複合バインド内で再割り当てする方向または全体の指定。
      bool active = false;                 ///< 更新と描画の候補として有効かどうか。
   };

   void BuildFallbackDefaults();
   bool LoadDefinitions(const std::filesystem::path& path, bool replaceAll);
   ActionDefinition* FindAction(const std::string& actionMap, const std::string& actionId);
   const ActionDefinition* FindAction(const std::string& actionMap, const std::string& actionId) const;
   bool BeginRebindPart(
      const std::string& actionMap, const std::string& actionId, size_t bindingIndex, RebindPart part);
   Vector2 EvaluateBinding(const InputBinding& binding, uint32_t playerSlot, float deadZone) const;
   static std::string MakeKey(const std::string& actionMap, const std::string& actionId);

   /// @brief 複合バインドを物理入力の列へ展開する。
   static std::vector<PhysicalControl> GetPhysicalControls(const InputBinding& binding);

   /// @brief 2つのバインドが同じ物理入力を使うか調べる。
   static bool SharesPhysicalControl(const InputBinding& left, const InputBinding& right);

   /// @brief 指定した物理入力がバインドに含まれるか調べる。
   static bool ContainsPhysicalControl(const InputBinding& binding, const PhysicalControl& target);

   /// @brief 複合バインド内の指定位置へ物理入力を設定する。
   static bool SetPhysicalControlAtIndex(InputBinding& binding, size_t controlIndex, const PhysicalControl& control);

   /// @brief 同じ形式のバインドで1つだけ変更された物理入力を取り出す。
   static bool FindSingleChangedControl(const InputBinding& previous, const InputBinding& replacement,
      PhysicalControl& previousControl, PhysicalControl& replacementControl);

   /// @brief 同じデバイス種別の物理入力をバインド内で置き換える。
   static bool ReplacePhysicalControl(
      InputBinding& binding, const PhysicalControl& target, const PhysicalControl& replacement);

   IInputBackend* backend_ = nullptr;   ///< 物理デバイス状態を取得する入力バックエンドへの非所有参照。
   std::filesystem::path defaultPath_;  ///< 既定の入力割り当てを読み込むファイル。
   std::filesystem::path overridePath_; ///< ユーザーの入力割り当てを保存・読み込みするファイル。
   /// @brief マップ名とアクションIDをキーにした現在の入力定義。
   std::unordered_map<std::string, ActionDefinition> actions_;
   std::unordered_map<std::string, ActionDefinition> defaults_; ///< 既定値へ戻すために保持する初期入力定義。
   PendingRebind pendingRebind_;                                ///< 次の物理入力を待っている再割り当て要求。
   InputBinding pendingConflictBinding_{}; ///< 既存割り当てと競合して確認待ちとなった入力バインド。
   bool hasPendingConflict_ = false;       ///< 再割り当てで解決待ちの競合があるかどうか。
   uint64_t frameNumber_ = 0;              ///< この入力状態を評価したフレーム番号。
};

} // namespace GameEngine
