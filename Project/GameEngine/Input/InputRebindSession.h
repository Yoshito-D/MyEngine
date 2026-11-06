#pragma once

#include "GameEngine/Input/InputBinding.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace GameEngine {

/// @brief 入力再割り当ての進行状態。描画の有無にかかわらずUpdateで進める。
enum class InputRebindState {
   Idle,
   WaitingForInput,
   WaitingForConflict,
   Committed,
   Cancelled
};

/// @brief 再割り当てするアクションと、そのバインド内の方向。
struct InputRebindTarget {
   std::string map;                    ///< 入力マップ名。
   std::string actionId;               ///< マップ内のアクションID。
   size_t bindingIndex = 0;            ///< バインドの添字。
   std::optional<size_t> controlIndex; ///< 未設定なら全体、設定時は複合軸内の方向。
};

/// @brief 入力待ち・競合確認・確定・中止だけを管理する再割り当てセッション。
class InputRebindSession {
public:
   /// @brief 候補の適用処理。第2引数がtrueなら競合先との置換を許可する。
   using Apply = std::function<bool(const InputBinding&, bool)>;

   /// @brief 現在の待受を置き換え、新しい対象への入力待ちを開始する。
   /// @param frameNumber 開始フレーム。このフレームのトリガーは捕捉しない。
   /// @return 対象の形式と方向が有効ならtrue。失敗時は現在のセッションを変更しない。
   bool Begin(InputRebindTarget target, InputBinding initialBinding, std::uint64_t frameNumber);

   /// @brief 新しい押下を捕捉して候補を適用する。競合時は候補を固定して確認を待つ。
   /// @param apply 候補の検証と適用を行う所有者側の処理。未設定なら中止する。
   void Update(IInputBackend& backend, std::uint64_t frameNumber, const Apply& apply);

   /// @brief 確認待ちの候補を競合置換ありで適用する。成功時だけCommittedへ進む。
   bool ConfirmConflict(const Apply& apply);

   /// @brief 競合した候補を破棄し、次フレームから入力待ちへ戻る。
   void RejectConflict(std::uint64_t frameNumber);

   /// @brief 保持する対象と候補を破棄し、Cancelledへ進む。
   void Cancel();

   /// @brief 現在の進行状態を取得する。
   InputRebindState GetState() const { return state_; }

   /// @brief 入力または競合確認を待っているか調べる。
   bool IsActive() const;

   /// @brief 対象の情報を取得する。IdleまたはCancelledなら未設定。
   const std::optional<InputRebindTarget>& GetTarget() const { return target_; }

   /// @brief 競合確認中の候補を取得する。それ以外では未設定。
   const std::optional<InputBinding>& GetCandidate() const { return candidate_; }

private:
   InputRebindState state_ = InputRebindState::Idle; ///< 現在の処理段階。
   std::optional<InputRebindTarget> target_;         ///< 待受対象。確定後は結果表示のため保持する。
   InputBinding initialBinding_;                     ///< 開始時の形式・倍率・未変更方向の設定。
   std::optional<InputBinding> candidate_;           ///< 競合確認に進んだ候補。
   std::uint64_t captureAfterFrame_ = 0;             ///< このフレームまでのトリガーを無視する。
};

} // namespace GameEngine
