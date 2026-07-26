#pragma once
#include "ParticleModule.h"
#include <algorithm>

namespace GameEngine {
/// @brief テクスチャシートのフレームをパーティクル寿命に同期して選択するモジュール
class TextureSheetAnimationModule : public ParticleModule {
public:
	/// @brief シート内で再生対象とするフレーム範囲
	enum class AnimationMode {
		WholeSheet = 0, ///< シート全体を行順に再生する
		SingleRow = 1  ///< 選択した1行のみを再生する
	};

	/// @brief 1×1シートを1周期再生する既定設定で構築する
	TextureSheetAnimationModule();

	/// @brief 開始フレームと必要に応じてランダムな行を初期化する
	/// @param particle 初期化するパーティクル
	void InitializeParticle(Particle& particle) const;
	/// @brief 寿命進行度から表示フレームを更新する
	/// @param particle 更新するパーティクル
	/// @param deltaTime 前回更新からの経過時間。寿命進行度を使うため現在は未使用
	void UpdateAnimation(Particle& particle, float deltaTime) const;

	/// @brief シートの横分割数を1以上に補正して設定する
	/// @param value 横方向のタイル数
	void SetTilesX(uint32_t value) { tilesX_ = std::max(value, 1u); ClampSettings(); }
	/// @brief シートの横分割数を取得する
	/// @return 横方向のタイル数
	uint32_t GetTilesX() const { return tilesX_; }
	/// @brief シートの縦分割数を1以上に補正して設定する
	/// @param value 縦方向のタイル数
	void SetTilesY(uint32_t value) { tilesY_ = std::max(value, 1u); ClampSettings(); }
	/// @brief シートの縦分割数を取得する
	/// @return 縦方向のタイル数
	uint32_t GetTilesY() const { return tilesY_; }
	/// @brief 寿命全体に対して進めるフレーム範囲の倍率を設定する
	/// @param value 0以上の再生倍率
	void SetFrameOverTime(float value) { frameOverTime_ = std::max(value, 0.0f); }
	/// @brief 寿命に対するフレーム進行倍率を取得する
	/// @return 再生倍率
	float GetFrameOverTime() const { return frameOverTime_; }
	/// @brief 寿命中の再生周期数を1以上に補正して設定する
	/// @param value 再生周期数
	void SetCycles(uint32_t value) { cycles_ = std::max(value, 1u); }
	/// @brief 寿命中の再生周期数を取得する
	/// @return 再生周期数
	uint32_t GetCycles() const { return cycles_; }
	/// @brief 開始位置から再生するフレーム数を設定する
	/// @param value フレーム数。0なら開始位置以降の全フレーム
	void SetFrameCount(uint32_t value) { frameCount_ = value; ClampSettings(); }
	/// @brief 明示された再生フレーム数を取得する
	/// @return フレーム数。0なら残り全フレーム
	uint32_t GetFrameCount() const { return frameCount_; }
	/// @brief 単一行モードで生成ごとに行を選ぶか設定する
	/// @param value ランダム選択を有効にする場合はtrue
	void SetRandomRow(bool value) { randomRow_ = animationMode_ == AnimationMode::SingleRow && value; }
	/// @brief ランダム行選択が有効か取得する
	/// @return 単一行モードでランダム選択する場合はtrue
	bool GetRandomRow() const { return randomRow_; }
	/// @brief 単一行モードで使用する固定行を設定する
	/// @param value 0始まりの行番号。シート範囲内へ補正される
	void SetRowIndex(uint32_t value) { rowIndex_ = value; ClampSettings(); }
	/// @brief 単一行モードの固定行を取得する
	/// @return 0始まりの行番号
	uint32_t GetRowIndex() const { return rowIndex_; }
	/// @brief 再生を始めるフレームを設定する
	/// @param value 0始まりのフレーム番号。再生範囲内へ循環される
	void SetStartFrame(uint32_t value) { startFrame_ = value; ClampSettings(); }
	/// @brief 再生開始フレームを取得する
	/// @return 0始まりのフレーム番号
	uint32_t GetStartFrame() const { return startFrame_; }
	/// @brief シート全体または単一行の再生モードを設定する
	/// @param value 使用する再生モード。不明値はシート全体へ補正される
	void SetAnimationMode(AnimationMode value) { animationMode_ = value == AnimationMode::SingleRow ? AnimationMode::SingleRow : AnimationMode::WholeSheet; ClampSettings(); }
	/// @brief 現在の再生モードを取得する
	/// @return シート全体または単一行モード
	AnimationMode GetAnimationMode() const { return animationMode_; }

	/// @copydoc ParticleModule::ToJson
	nlohmann::json ToJson() const override;
	/// @copydoc ParticleModule::FromJson
	void FromJson(const nlohmann::json& json) override;

#ifdef USE_IMGUI
	/// @copydoc ParticleModule::DrawInspector
	void DrawInspector() override;
#endif

private:
	void ClampSettings();
	uint32_t GetTotalFrameCount() const;
	uint32_t GetPlayableFrameCount() const;
	uint32_t ResolveFrame(const Particle& particle) const;

	uint32_t tilesX_ = 1;
	uint32_t tilesY_ = 1;
	float frameOverTime_ = 1.0f;
	uint32_t cycles_ = 1;
	uint32_t frameCount_ = 0;
	bool randomRow_ = false;
	uint32_t rowIndex_ = 0;
	uint32_t startFrame_ = 0;
	AnimationMode animationMode_ = AnimationMode::WholeSheet;
};
}
