#pragma once
#include "IObjectComponent.h"
#include "Utility/VectorMath.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace GameEngine {
class ParticleSystem;

/// @brief GameObject に ParticleSystem を接続するコンポーネント
/// ParticleSystem の生成・再生制御と Object トランスフォームへの追従を担う
class ParticleEmitterComponent final : public IObjectComponent {
public:
   static constexpr const char* kTypeName = "ParticleEmitterComponent";
   static constexpr ComponentDisplayName kDisplayName{ "パーティクルエミッター", "Particle Emitter" };
   const char* GetTypeName() const override;

   // ── 追従・オフセット設定 ──────────────────────────

   /// @brief 追従・オフセット設定をまとめた構造体
   struct AttachmentConfig {
	  bool followPosition = true;   ///< 位置を追従するか
	  bool followRotation = true;   ///< 回転を追従するか
	  bool followScale = true;  ///< スケールを追従するか（デフォルト off）

	  Vector3 positionOffset = { 0.0f, 0.0f, 0.0f };  ///< 位置オフセット
	  Vector3 rotationOffset = { 0.0f, 0.0f, 0.0f };  ///< 回転オフセット（ラジアン）
	  Vector3 scaleOffset = { 1.0f, 1.0f, 1.0f };  ///< スケールオフセット

	  /// @brief シミュレーション空間
	  enum class Space { World, Local } simulationSpace = Space::World;

	  /// @brief 追従するボーン名（空文字列 = ルート TransformComponent を使用）
	  std::string boneName;
   };

   // ── エミッタースロット ────────────────────────────

   /// @brief 1スロット分のエフェクト設定と再生状態を保持する構造体
   struct EmitterSlot {
	  std::string        jsonPath;              ///< エフェクト JSON ファイルパス
	  AttachmentConfig   attachConfig;          ///< 追従設定（スロット個別）
	  bool               autoPlay = true;   ///< スロット有効化時に自動再生
	  bool               loop = true;   ///< ループ再生するか
	  bool               playOnceAndDestroy = false;  ///< 終了後にスロットを除去するか

	  /// @brief スロット終了時に呼ばれるコールバック
	  std::function<void(int slotIndex)> onFinished;

	  /// @brief 管理している ParticleSystem（実行時に生成される）
	  std::shared_ptr<ParticleSystem> particleSystem;
   };

   // ── ライフサイクル ────────────────────────────────
   void OnAttach() override;
   void OnDetach() override;
   void OnEnable() override;
   void OnDisable() override;
   void Update(float) override;

   // ── スロット管理 ──────────────────────────────────

   /// @brief スロットを追加して JSON からエフェクトを読み込む
   /// @param jsonPath エフェクト JSON ファイルパス
   /// @param config   追従設定（省略時はデフォルト）
   /// @return 追加したスロットのインデックス
   int  AddSlot(const std::string& jsonPath, const AttachmentConfig& config = {});

   /// @brief スロットを削除する
   /// @param slotIndex 削除するスロットのインデックス
   void RemoveSlot(int slotIndex);

   /// @brief 全スロットを削除する
   void ClearSlots();

   /// @brief 管理中のParticleSystemを自動描画/更新対象から外す
   void UnregisterParticleSystemsForRender();

   /// @brief スロット数を取得する
   int  GetSlotCount() const { return static_cast<int>(slots_.size()); }

   /// @brief スロットを取得する（書き込み可）
   EmitterSlot* GetSlot(int slotIndex);

   /// @brief スロットを取得する（読み取り専用）
   const EmitterSlot* GetSlot(int slotIndex) const;

   // ── 後方互換ヘルパー（スロット 0 を操作） ────────

   /// @brief JSON ファイルからエフェクトを読み込み、スロット 0 に設定する
   bool LoadEffect(const std::string& jsonPath);

   /// @brief 外部から生成済みの ParticleSystem をスロット 0 に注入する
   void SetParticleSystem(std::shared_ptr<ParticleSystem> ps);

   /// @brief スロット 0 の ParticleSystem を取得する
   ParticleSystem* GetParticleSystem() const;

   // ── 一括再生制御（全スロット） ────────────────────
   void Play();
   void Stop();
   void Pause();
   void Resume();
   void Restart();

   bool IsPlaying()  const;  ///< 少なくとも 1 スロットが再生中なら true
   bool IsFinished() const;  ///< 全スロットが終了していれば true

   // ── 個別スロット再生制御 ──────────────────────────
   void Play(int slotIndex);
   void Stop(int slotIndex);
   void Pause(int slotIndex);
   void Resume(int slotIndex);
   void Restart(int slotIndex);

   bool IsPlaying(int slotIndex) const;
   bool IsFinished(int slotIndex) const;

   // ── コールバック（全スロット終了時） ──────────────
   /// @brief 全スロットが終了したとき（IsFinished() が true になったとき）に呼ばれる
   std::function<void()> onFinished;

   // ── コンポーネント共通オプション ─────────────────
   float maxCullDistance = 0.0f;  ///< カリング距離（0 = 無効）

   // ── シリアライズ ──────────────────────────────────
   nlohmann::json Serialize() const override;
   void Deserialize(const nlohmann::json& data) override;

   /// @brief 指定スロットの ParticleSystem を再生成する（loop / autoPlay 変更後に呼ぶ）
   bool LoadSlot(EmitterSlot& slot);

#ifdef USE_IMGUI
   void DrawInspector() override;
#endif

private:
   /// @brief スロット用エミッター行列を計算する
   Matrix4x4 ComputeEmitterMatrix(const AttachmentConfig& cfg) const;

   /// @brief 計算済みエミッター行列を ShapeModule の Transform に反映する
   static void ApplyEmitterToShapeModule(ParticleSystem* ps, const Matrix4x4& emitterMatrix);

   /// @brief simulationSpace を ParticleSystem 側に伝播する
   static void SyncSimulationSpace(ParticleSystem* ps, AttachmentConfig::Space space);

   std::vector<EmitterSlot> slots_;

   // 後方互換のために保持するデフォルト AttachmentConfig（スロット 0 用）
   std::string legacyJsonPath_;
};

} // namespace GameEngine
