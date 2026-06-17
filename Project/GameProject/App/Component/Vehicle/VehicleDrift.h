#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/Math/Vector3.h"

namespace App {

/// @brief ドリフト処理を担うコンポーネント
///
/// 【概要】
///   VehicleGroundMover が提供する直進走行に対して、
///   意図的に後輪を滑らせてコーナリングする「ドリフト」を実現する。
///
/// 【ドリフト移行の 2 モード】
///   ButtonMode      … 専用ボタン（Q / LB）を押し続けると即ドリフト開始
///   SustainedSteer  … 同方向のステア入力を sustainedSteerTime 秒以上継続で自動移行
///                     追加ボタン不要でよりリアルな「滑り込み」感を表現できる。
///
/// 【リアル横滑り物理】
///   slideLateralSpeed_（横速度スカラー）をフレームをまたいで保持し、
///   ドリフト中は lateralBuildupRate で目標値へ滑らかに収束させる。
///   ドリフト終了後も postDriftBleedRate で指数減衰し、タイヤが地面に
///   グリップを取り戻す「引っかかり」感を表現する。
///
/// 【ミニターボ（オプション）】
///   miniTurboEnabled = true のとき、ドリフト継続時間が miniTurboMinTime を
///   超えた状態でドリフトを終了すると miniTurboBoost を加算してブーストする。
///   false にすればボーナスなしのシンプルなドリフトになる。
///
/// このコンポーネントを Object にアタッチするだけで機能が有効化される。
/// 外せばドリフトなしの通常走行に戻る。
class VehicleDrift final : public GameEngine::IObjectComponent {
public:
   /// @brief ドリフト移行方式
   enum class DriftMode : int {
	  ButtonMode     = 0,  ///< 専用ボタンを押し続けることでドリフト開始
	  SustainedSteer = 1,  ///< 同方向ステアを一定時間継続することでドリフト開始
   };

   static constexpr const char* kTypeName = "VehicleDrift";
   const char* GetTypeName() const override { return kTypeName; }
   void Update(float deltaTime) override { (void)deltaTime; }

   /// @brief ドリフト入力・継続判定・速度横滑りを一括処理する
   /// @param driftInput  ドリフトボタン入力（ButtonMode で使用）
   /// @param steerInput  左右入力（-1〜+1）
   /// @param gravityUp   現在の重力Up方向（速度の垂直成分を保持するために必要）
   /// @param deltaTime   フレーム時間
   void Apply(bool driftInput, float steerInput,
			  const GameEngine::Vector3& gravityUp, float deltaTime);

   /// @brief ドリフト中かどうかを返す
   bool IsDrifting() const { return isDrifting_; }

   /// @brief ミニターボが発動できるかどうかをかえす
   bool CanFireMiniTurbo() const { return miniTurboEnabled && driftTimer_ >= miniTurboMinTime; }

   /// @brief ミニターボが発動したか確認し、フラグを消費して返す（1フレームに1回だけ true）
   bool ConsumeMiniTurboFired() {
      if (miniTurboJustFired_) { miniTurboJustFired_ = false; return true; }
      return false;
   }

#ifdef USE_IMGUI
   void DrawInspector() override;
#endif

   nlohmann::json Serialize() const override;
   void Deserialize(const nlohmann::json& data) override;

public:
   // ----------------------------------------------------------------
   // 調整パラメータ
   // ----------------------------------------------------------------

   /// @brief ドリフト移行方式（デフォルト: SustainedSteer = 追加ボタン不要）
   DriftMode driftMode = DriftMode::SustainedSteer;

   /// @brief 横滑りの最大量（0=前方のみ / 1=完全横向き）
   /// ドリフト中に slideLateralSpeed_ がこの割合まで滑らかに増える。
   float slideRatio = 0.2f;

   /// @brief 横速度の蓄積速度（/sec）
   /// 大きいほどドリフト開始直後にすぐ滑り始める。小さいほどゆっくり滑り込む。
   float lateralBuildupRate = 1.0f;

   /// @brief ドリフト終了後の横速度減衰速度（/sec）
   /// 大きいほどすぐにグリップが戻る。小さいほど余韻が長く残る。
   float postDriftBleedRate = 6.0f;

   /// @brief ドリフト中のステアリング増幅率
   /// 通常ステアより鋭くなることで、ドリフト中の細かい向き修正が可能になる。
   float driftSteerMult = 1.5f;

   /// @brief ミニターボを有効にするか
   /// false にするとドリフト終了時の速度ブーストが発動しない。
   bool miniTurboEnabled = true;

   /// @brief ドリフト終了後に加算する速度ブースト量（units/sec）
   float miniTurboBoost = 30.0f;

   /// @brief ミニターボが発動するドリフト最低継続時間（秒）
   float miniTurboMinTime = 0.5f;

   /// @brief SustainedSteer モードで同方向入力を継続する必要がある時間（秒）
   float sustainedSteerTime = 0.2f;

   /// @brief ドリフト発動に必要な最低ステア絶対値（デッドゾーン）
   float driftSteerDeadZone = 0.3f;

private:
   // ----------------------------------------------------------------
   // 入力判定ヘルパー
   // ----------------------------------------------------------------

   /// @brief 現在フレームでドリフト開始条件を満たすか判定する
   bool ShouldStartDrift(bool driftInput, float steerInput) const;

   /// @brief 現在フレームでドリフトを継続できるか判定する
   /// ドリフト中は入力が離れたら終了する
   bool ShouldContinueDrift(bool driftInput, float steerInput) const;

   // ----------------------------------------------------------------
   // 物理ヘルパー
   // ----------------------------------------------------------------

   /// @brief ドリフト中の速度横滑りを GravityBody へ書き込む
   /// 横速度を瞬時に設定せず lateralBuildupRate で滑らかに目標値へ近づける。
   /// @param steerInput  現在のステア入力（横方向符号の決定に使用）
   /// @param gravityUp   重力Up（垂直速度成分を維持するため）
   /// @param deltaTime   フレーム時間
   void ApplySlideVelocity(float steerInput, const GameEngine::Vector3& gravityUp, float deltaTime);

   /// @brief ドリフト終了後に残留横速度を指数減衰させ GravityBody へ書き込む
   /// これにより「タイヤがグリップを取り戻す」余韻が生まれる。
   /// @param gravityUp   重力Up
   /// @param deltaTime   フレーム時間
   void ApplyPostDriftBleed(const GameEngine::Vector3& gravityUp, float deltaTime);

   /// @brief ドリフト終了時にミニターボを発動する（有効かつ継続時間が十分なら）
   void TryFireMiniTurbo();

   // ----------------------------------------------------------------
   // 状態
   // ----------------------------------------------------------------

   /// @brief 現在ドリフト中かどうか
   bool  isDrifting_ = false;

   /// @brief ミニターボが発動したフレームに true になる（ConsumeMiniTurboFired で消費）
   bool  miniTurboJustFired_ = false;

   /// @brief ドリフト継続時間（秒）。開始時にリセットされる。
   float driftTimer_ = 0.0f;

   /// @brief ドリフトしている方向の符号（+1 右 / -1 左）。速度・継続判定に使用。
   float driftSign_  = 0.0f;

   /// @brief 現在の横速度（units/sec）。ドリフト中に蓄積し終了後に減衰する。
   float slideLateralSpeed_ = 0.0f;

   /// @brief SustainedSteer モード用: 同方向ステア入力の蓄積時間（秒）
   float sustainedTimer_ = 0.0f;

   /// @brief SustainedSteer モード用: 直前フレームのステア入力符号
   float prevSteerSign_ = 0.0f;
};

} // namespace App
