#pragma once
#include <string>
#include <cstdint>

namespace GameEngine {
class UITextComponent;
}

namespace App {
enum class LandingResult;
/// @brief チュートリアルで進行・表示に共有する課題の識別子。
enum class TutorialPhase {
   DrivingIntro,
   DriftIntro,
   DriftPractice,
   JumpIntro,
   JumpPractice,
   AirControlIntro,
   AirControlPractice,
   GoalGuidanceIntro,
   PlanetTransfer,
   Goal,
   Complete,
};

/// @brief 進行側が確定した表示用スナップショット。コンポーネント参照を持たない。
struct TutorialGuideState {
   TutorialPhase phase = TutorialPhase::DrivingIntro; ///< 現在の課題。
   bool gamepadConnected = false;                     ///< 操作説明に使う入力方式。
   int32_t miniTurboCount = 0;                        ///< 達成回数。
   bool canFireMiniTurbo = false;                     ///< ターボ発動可能か。
   bool isDrifting = false;                           ///< ドリフト中か。
   bool isAirborne = false;                           ///< 空中か。
   bool aligned = false;                              ///< 機体の底が地表と平行か。
   bool airRotationPerformed = false;                 ///< 空中回転の練習を達成したか。
};

/// @brief 案内・結果文面とフェードを管理し、課題の進行判定から分離する。
class TutorialGuidePresenter {
public:
   /// @brief 案内履歴を消し、シーンの本文と補足文の基準不透明度を保存する。
   void Reset(float baseOpacity, float feedbackBaseOpacity);

   /// @brief 説明が完全表示され、次へ進む入力を受け付けられるかを返す。
   bool IsReady() const { return guideFadeState_ == GuideFadeState::Visible && pendingText_.empty(); }

   /// @brief 最新の課題文面に向け、本文・操作案内・補足文のフェードを同期する。本文の表示先がない場合は何もしない。
   void UpdateGuideText(const TutorialGuideState& state, GameEngine::UITextComponent* guideText,
      GameEngine::UITextComponent* promptText, GameEngine::UITextComponent* feedbackText, float deltaTime,
      float fadeDuration);

   /// @brief 折り返しを含む本文と操作案内の実際の高さに合わせ、各UIを余白付きで縦に並べる。
   void UpdateLayout(GameEngine::UITextComponent* guideText, GameEngine::UITextComponent* promptText,
      GameEngine::UITextComponent* feedbackText);
   /// @brief 本文のフェードアウト中は前の補足文を保ち、それ以外は最新の操作状態と結果を表示する。
   void UpdateFeedback(const TutorialGuideState& state, GameEngine::UITextComponent* feedbackText, float deltaTime);
   /// @brief ドリフト終了の成否を一定時間表示する。
   void ShowDriftResult(bool fired);
   /// @brief 着地品質に対応する結果文面を更新する。
   void ShowLandingResult(LandingResult result);
   /// @brief 元の惑星に戻った際の再挑戦案内を追加する。
   void ShowReturnToSource();

   /// @brief 前の着地結果を消去する。
   void ClearLandingFeedback() { landingFeedback_.clear(); }

private:
   using Phase = TutorialPhase;
   enum class GuideFadeState {
      Visible,
      FadingOut,
      FadingIn,
   };

   static std::string BuildGuideText(const TutorialGuideState& state);
   void ApplyGuideOpacity(GameEngine::UITextComponent* guideText, GameEngine::UITextComponent* promptText,
      GameEngine::UITextComponent* feedbackText);
   GuideFadeState guideFadeState_ = GuideFadeState::Visible; ///< 案内文のフェード状態。
   std::string displayedText_;                               ///< 最後に反映した案内。
   std::string pendingText_;                                 ///< フェード後に反映する案内。
   std::string landingFeedback_;                             ///< 着地の結果に対応する説明。
   std::string driftFeedback_;                               ///< 直近のドリフト結果。
   float driftFeedbackRemaining_ = 0.0f;                     ///< 操作結果を表示する残り時間。進行判定には使わない。
   float guideBaseOpacity_ = 1.0f;                           ///< シーンに保存された案内文の不透明度。
   float feedbackBaseOpacity_ = 1.0f;                        ///< シーンに保存された補足文の不透明度。
   float guideVisibility_ = 1.0f;                            ///< フェードによる表示割合。
};
}
