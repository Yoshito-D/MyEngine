#pragma once
#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameEngine/Math/VectorMath.h"
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
   /// @brief 所有Systemを持たないエミッターを構築する。
   ParticleEmitterComponent();
   /// @brief 所有Systemを確実に停止・破棄する。
   ~ParticleEmitterComponent() override;

   // ── 追従・オフセット設定 ──────────────────────────

   /// @brief 追従・オフセット設定をまとめた構造体
   struct AttachmentConfig {
	  bool followPosition = true;   ///< 位置を追従するか
	  bool followRotation = true;   ///< 回転を追従するか
	  bool followScale = true;  ///< スケールを追従するか（デフォルト on）

	  Vector3 positionOffset = { 0.0f, 0.0f, 0.0f };  ///< 位置オフセット
	  Vector3 rotationOffset = { 0.0f, 0.0f, 0.0f };  ///< 回転オフセット（ラジアン）
	  Vector3 scaleOffset = { 1.0f, 1.0f, 1.0f };  ///< スケールオフセット

	  /// @brief シミュレーション空間
	  enum class Space { World, Local } simulationSpace = Space::World;

	  /// @brief 追従するボーン名（空文字列 = ルート TransformComponent を使用）
	  std::string boneName;
   };

   // ── エミッタースロット ────────────────────────────

   /// @brief 保存・編集するスロット設定。実行中のSystemは含まない。
   struct SlotConfiguration {
      std::string jsonPath;
      AttachmentConfig attachConfig;
      bool autoPlay = true;
      bool loop = true;
      bool playOnceAndDestroy = false;
   };

   // ── ライフサイクル ────────────────────────────────
   /// @copydoc IObjectComponent::OnAttach
   void OnAttach() override;
   /// @copydoc IObjectComponent::OnDetach
   void OnDetach() override;
   /// @copydoc IObjectComponent::OnEnable
   void OnEnable() override;
   /// @copydoc IObjectComponent::OnDisable
   void OnDisable() override;
   /// @copydoc IObjectComponent::Update
   void Update(float) override;

   /// @brief 選択中のジョイントとエミッター位置をデバッグ描画する
   void DrawDebugAttachments() const;

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

   /// @brief 実体を公開せず、指定スロットの設定を参照する。
   const SlotConfiguration* DescribeSlot(int slotIndex) const;
   /// @brief 設定の置換とSystemの読み込み・追従・再生の同期を完了する。
   bool ConfigureSlot(int slotIndex, const SlotConfiguration& configuration);
   /// @brief 生存粒子を維持したまま、指定スロットの新規放出を切り替える。
   void EnableEmission(int slotIndex, bool enabled);
   /// @brief 指定スロットが表示中の粒子を持つか調べる。
   bool HasLivingParticles(int slotIndex) const;

   // ── 後方互換ヘルパー（スロット 0 を操作） ────────

   /// @brief JSON ファイルからエフェクトを読み込み、スロット 0 に設定する
   bool LoadEffect(const std::string& jsonPath);

   // ── 一括再生制御（全スロット） ────────────────────
   /// @brief 全スロットの再生を開始する
   void Play();
   /// @brief 全スロットを停止し、再生位置を先頭へ戻す
   void Stop();
   /// @brief 再生中の全スロットを一時停止する
   void Pause();
   /// @brief 一時停止中の全スロットを再開する
   void Resume();
   /// @brief 全スロットを先頭から再生し直す
   void Restart();

   /// @brief いずれかのスロットが再生中かを判定する
   /// @return 少なくとも1スロットが再生中の場合はtrue
   bool IsPlaying()  const;  ///< 少なくとも 1 スロットが再生中なら true
   /// @brief 全スロットが再生を完了したかを判定する
   /// @return 全スロットが完了している場合はtrue
   bool IsFinished() const;  ///< 全スロットが終了していれば true

   // ── 個別スロット再生制御 ──────────────────────────
   /// @brief 指定スロットの再生を開始する
   void Play(int slotIndex);
   /// @brief 指定スロットを停止し、再生位置を先頭へ戻す
   void Stop(int slotIndex);
   /// @brief 指定スロットを一時停止する
   void Pause(int slotIndex);
   /// @brief 指定スロットの一時停止を解除する
   void Resume(int slotIndex);
   /// @brief 指定スロットを先頭から再生し直す
   void Restart(int slotIndex);

   /// @brief 指定スロットが再生中かを判定する
   /// @return スロットが存在し再生中の場合はtrue
   bool IsPlaying(int slotIndex) const;
   /// @brief 指定スロットが再生を完了したかを判定する
   /// @return スロットが存在し完了している場合はtrue
   bool IsFinished(int slotIndex) const;

   /// @brief スロットのエミッターを指定ワールド座標に固定する
   /// @param slotIndex 対象スロット
   /// @param position ワールド位置
   /// @param rotation ワールド回転
   /// @param scale ワールドスケール
   void SetSlotWorldTransform(
	  int slotIndex,
	  const Vector3& position,
	  const Quaternion& rotation,
	  const Vector3& scale = { 1.0f, 1.0f, 1.0f });

   // ── シリアライズ ──────────────────────────────────
   /// @copydoc IObjectComponent::Serialize
   nlohmann::json Serialize() const override;
   /// @copydoc IObjectComponent::Deserialize
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @copydoc IObjectComponent::DrawInspector
   void DrawInspector() override;
#endif

private:
   struct EmitterSlot : SlotConfiguration {
      std::function<void(int)> onFinished;
      std::unique_ptr<ParticleSystem> particleSystem;
   };
   EmitterSlot* GetSlot(int slotIndex);
   const EmitterSlot* GetSlot(int slotIndex) const;
   bool LoadSlot(EmitterSlot& slot);
   static SlotConfiguration ValidateConfiguration(SlotConfiguration configuration);
   // ── コールバック（全スロット終了時） ──────────────
   /// @brief 全スロットが終了したとき（IsFinished() が true になったとき）に呼ばれる
   std::function<void()> onFinished;

   // ── コンポーネント共通オプション ─────────────────
   float maxCullDistance = 0.0f;  ///< カリング距離（0 = 無効）
   bool debugDrawAttachments = true; ///< ジョイントのアタッチ位置をデバッグ描画するか

   /// @brief 指定ジョイントの現在のワールド行列を取得する
   bool TryComputeJointWorldMatrix(const std::string& jointName, Matrix4x4& jointWorldMatrix) const;

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
