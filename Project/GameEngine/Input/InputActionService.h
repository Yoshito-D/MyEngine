#pragma once

#include "GameEngine/Input/InputBinding.h"
#include "GameEngine/Input/InputRebindSession.h"
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace GameEngine {

/// @brief 入力アクションが返す値の種類。
enum class InputActionType {
   Button,
   Axis1D,
   Axis2D
};

/// @brief 一フレーム分の入力アクション状態。
struct InputActionState {
   Vector2 value{};               ///< ボタンと一次元軸はX成分を使う評価済み入力値。
   bool held = false;             ///< 現在入力が押下状態か。
   bool triggered = false;        ///< このフレームで押下へ変化したか。
   bool released = false;         ///< このフレームで解放へ変化したか。
   std::uint64_t frameNumber = 0; ///< 評価したフレームの連番。
};

/// @brief 物理入力をゲーム用アクションへ変換し、設定と再割り当てセッションを所有する。
class InputActionService final {
public:
   static constexpr std::uint32_t kMaxPlayers = 4; ///< 入力状態を独立して保持するプレイヤースロット数。

   /// @brief バックエンド・設定ファイルを指定し、既定値とユーザー設定を読み込む。
   /// @param backend 低レベル入力バックエンド。サービスの利用中は生存させる。
   /// @param defaultPath 既定アクション設定。
   /// @param overridePath ユーザー上書き設定。
   void Initialize(
      IInputBackend& backend, const std::filesystem::path& defaultPath, const std::filesystem::path& overridePath);

   /// @brief アクションのスナップショットを確定した後、描画から独立して再割り当てを進める。
   void Update();

   /// @brief 指定プレイヤーの評価済みアクション状態を取得する。見つからない場合はゼロ状態。
   const InputActionState& GetActionState(
      const std::string& actionMap, const std::string& actionId, std::uint32_t playerSlot = 0) const;

   /// @brief 再割り当ての待受を開始する。開始フレームのトリガーは採用しない。
   /// @param controlIndex 未設定なら全体、設定時はキーバインドの物理入力列内の方向。
   /// @return アクション・添字・指定方向が有効ならtrue。
   bool BeginRebind(const std::string& actionMap, const std::string& actionId, size_t bindingIndex,
      std::optional<size_t> controlIndex = std::nullopt);

   /// @brief 再割り当ての状態・対象・競合候補を読み取る。
   const InputRebindSession& GetRebindSession() const { return rebindSession_; }

   /// @brief 確認待ちの候補を競合置換ありで適用する。適用できなければ確認待ちを維持する。
   bool ConfirmRebindConflict();

   /// @brief 競合候補を破棄し、次フレームから入力待ちへ戻す。
   void RejectRebindConflict();

   /// @brief 待受と競合確認を中止し、対象と候補を破棄する。
   void CancelRebind();

   /// @brief 指定アクションへバインドを適用する。
   /// @details 物理入力の競合は同一アクションマップ内でのみ検出・置換する。
   /// @return バインドが有効で、競合がないか置換が許可された場合はtrue。
   bool ApplyBinding(const std::string& actionMap, const std::string& actionId, size_t bindingIndex,
      const InputBinding& binding, bool replaceConflict);

   /// @brief ユーザー上書き設定を読み込む。成功した設定変更では進行中の再割り当てを中止する。
   /// @return ファイル全体が正常だった場合はtrue。
   bool LoadOverrides();

   /// @brief 現在の設定を一時ファイル経由で原子的に保存する。
   /// @return 保存できた場合はtrue。
   bool SaveOverrides() const;

   /// @brief 全割り当てを既定値へ戻し、進行中の再割り当てを中止する。
   void ResetToDefaults();

#ifdef USE_IMGUI
   /// @brief 入力設定の状態を描画し、開始・確認・取消などの操作要求だけを送る。
   void DrawImGui();
#endif

private:
   struct ActionDefinition {
      std::string map;                                    ///< 入力マップ名。
      std::string id;                                     ///< マップ内のアクションID。
      InputActionType type = InputActionType::Button;     ///< 合成するアクション値の種類。
      float deadZone = 0.24f;                             ///< スティックを中立とみなす範囲。
      std::vector<InputBinding> bindings;                 ///< 物理入力の割り当て。
      std::array<InputActionState, kMaxPlayers> states{}; ///< プレイヤーごとの評価済み状態。
   };

   void BuildFallbackDefaults();
   bool LoadDefinitions(const std::filesystem::path& path, bool replaceAll);
   ActionDefinition* FindAction(const std::string& actionMap, const std::string& actionId);
   const ActionDefinition* FindAction(const std::string& actionMap, const std::string& actionId) const;
   void UpdateRebind();
   bool HasRebindTarget() const;
   bool ApplyRebindCandidate(const InputBinding& binding, bool replaceConflict);
   static std::string MakeKey(const std::string& actionMap, const std::string& actionId);
   static bool FindSingleChangedControl(const InputBinding& previous, const InputBinding& replacement,
      InputPhysicalControl& previousControl, InputPhysicalControl& replacementControl);

   IInputBackend* backend_ = nullptr;                           ///< 非所有の物理入力バックエンド。
   std::filesystem::path defaultPath_;                          ///< 既定入力定義の読み込み元。
   std::filesystem::path overridePath_;                         ///< ユーザー入力定義の読み書き先。
   std::unordered_map<std::string, ActionDefinition> actions_;  ///< 現在の入力定義。
   std::unordered_map<std::string, ActionDefinition> defaults_; ///< リセット先となる既定入力定義。
   InputRebindSession rebindSession_;                           ///< 描画に依存しない再割り当ての状態。
   std::uint64_t frameNumber_ = 0;                              ///< 評価したフレームの連番。
};

} // namespace GameEngine
