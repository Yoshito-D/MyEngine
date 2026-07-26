#pragma once
#include "Utility/VectorMath.h"
#include "Utility/MathUtils.h"
#include "Particle.h"
#include "ParticleMaterial.h"
#include "Module/MainModule.h"
#include "Module/EmissionModule.h"
#include "Module/ShapeModule.h"
#include "Module/LifetimeModules.h"
#include "Module/RendererModule.h"
#include "Module/TrailModule.h"
#include "Module/ParticleMeshModule.h"
#include "Core/Graphics/Mesh.h"
#include "Core/Graphics/Texture.h"
#include "Core/Graphics/Material.h"
#include "Object/Model/ModelAsset.h"
#include <nlohmann/json.hpp>
#include <array>
#include <deque>
#include <optional>
#include <stack>
#include <memory>
#include <vector>
#include <wrl.h>

namespace GameEngine {
class GraphicsDevice;
class Camera;
class PSOManager;

/// @brief Unity ライクなパーティクルシステム
class ParticleSystem {
public:
   static constexpr uint32_t kMaxParticles = 4096;

   /// @brief 親粒子イベントから生成する子エフェクト設定
   struct SubEmitterSettings {
	  bool enabled = false;
	  std::string spawnOnDeathPath;
	  std::string spawnOnUpdatePath;
	  std::string spawnOnCollisionPath;
	  float updateInterval = 0.1f;
	  uint32_t maxEventsPerFrame = 32;
	  Vector3 collisionPlaneNormal{ 0.0f, 1.0f, 0.0f };
	  float collisionPlaneDistance = 0.0f;
	  float collisionRestitution = 0.0f;
   };

   /// @brief 自動更新・描画対象として登録された全システムを取得する
   /// @return 生存中の登録システム一覧
   static const std::vector<ParticleSystem*>& GetRegisteredParticleSystems();

   /// @brief 指定システムを自動更新・描画一覧から外す
   /// @param particleSystem 解除対象
   static void UnregisterParticleSystem(ParticleSystem* particleSystem);

   /// @brief 自動更新・描画レジストリを空にする
   static void ClearRegisteredParticleSystems();

   /// @brief 更新中に蓄積したサブエミッターイベントを安全なタイミングで生成する
   static void ProcessPendingSubEmitters();

   /// @brief GPU送信用パーティクルデータ
   struct ParticleForGPU {
	  Matrix4x4 wvp;
	  Matrix4x4 world;
	  Matrix4x4 uvTransform;
	  Vector4 color;
	  Vector4 customData;
   };

   /// @brief Compute Shader が更新する最小シミュレーション状態
   struct GpuParticleState {
	  Vector4 positionAndActive;
	  Vector4 velocityAndLifetime;
	  uint32_t ownerParticleIndex = UINT_MAX;
	  float age = 0.0f;
	  float initialLifetime = 0.0f;
	  uint32_t padding = 0;
   };

   /// @brief Update CSが粒子ごとに参照する永続的な運動パラメータ
   struct GpuParticleMotion {
	  Vector4 forceAndDrag;
	  Vector4 velocityAndSpeedModifier;
	  Vector4 limitAndGravity;
	  Vector4 noiseParams;
   };

   /// @brief CPUで初期化した粒子固有値をGPUシミュレーションへ渡す属性
   struct GpuParticleAttributes {
	  Matrix4x4 uvTransform;
	  Vector4 color;
	  Vector4 sizeAndRotation;
	  Vector4 rotationQuaternion;
	  Vector4 customData;
	  uint32_t stateIndex = 0; // 圧縮済み描画属性から永続GPU状態を引くための元粒子番号
	  uint32_t padding[3]{};
   };

   /// @brief Emitter CSへ渡す生成または状態上書き要求
   struct GpuSpawnRequest {
	  GpuParticleState state;
	  GpuParticleMotion motion;
	  uint32_t operation = 0; // 0: FreeListから生成、1: 既存所有粒子を上書き
	  uint32_t padding[3]{};
   };

   /// @brief GPUシミュレーションのフレーム定数
   struct GpuSimulationSettings {
	  Matrix4x4 viewProjection;
	  Vector4 cameraPosition;
	  Vector4 cameraRight;
	  Vector4 cameraUp;
	  Vector4 cameraForward;
	  Vector4 renderParams;
	  Vector4 cameraFadeParams;
	  Vector4 attractorPosition;
	  Vector4 attractorParams;
	  Vector4 simulationOriginAndLocal;
	  Vector4 simulationRotation;
	  uint32_t particleCount = 0;
	  uint32_t particleCapacity = 0;
	  uint32_t spawnRequestCount = 0;
	  uint32_t initializationStartIndex = 0;
   };

   /// @brief リボン1区間をCompute Shaderへ渡す入力
   struct GpuRibbonSegment {
	  Vector4 startAndWidth;
	  Vector4 endAndWidth;
	  Vector4 startTangentAndV;
	  Vector4 endTangentAndV;
	  Vector4 alphaAndPadding;
   };

   /// @brief シミュレーション空間
   enum class SimulationSpace {
	  World,  // ワールド空間
	  Local   // ローカル空間
   };

   /// @brief 初期化（静的リソース）
   static void Initialize(GraphicsDevice* device);

   /// @brief コンストラクタ
   ParticleSystem();

   /// @brief デストラクタ（SRVデスクリプタを解放）
   ~ParticleSystem();

   /// @brief パーティクルシステムの作成
   void Create();

   /// @brief 更新処理
   /// @param deltaTime デルタタイム
   void Update(float deltaTime);

   /// @brief 行列更新（カメラ情報が必要）
   /// @param camera カメラ
   void UpdateMatrix(Camera* camera);

   /// @brief 再生開始
   void Play();

   /// @brief 停止
   void Stop();

   /// @brief 一時停止
   void Pause();

   /// @brief 一時停止から再開
   void Resume();

   /// @brief 再生中か判定
   bool IsPlaying() const { return isPlaying_; }

   /// @brief 終了しているか判定（非ループかつ duration を超えた場合 true）
   bool IsFinished() const;

   /// @brief 寿命・初期値・再生条件を管理するメインモジュールを取得する
   /// @return 常に有効なメインモジュール
   MainModule* GetMainModule() { return mainModule_.get(); }
   /// @brief 時間・距離・バースト放出を管理するモジュールを取得する
   /// @return 常に有効な放出モジュール
   EmissionModule* GetEmissionModule() { return emissionModule_.get(); }
   /// @brief 放出位置と初期方向を管理する形状モジュールを取得する
   /// @return 常に有効な形状モジュール
   ShapeModule* GetShapeModule() { return shapeModule_.get(); }

   /// @brief 寿命中の速度変更モジュールを取得する
   /// @return 速度モジュール
   VelocityOverLifetimeModule* GetVelocityOverLifetimeModule() { return velocityOverLifetimeModule_.get(); }
   /// @brief 寿命中の色変更モジュールを取得する
   /// @return 色モジュール
   ColorOverLifetimeModule* GetColorOverLifetimeModule() { return colorOverLifetimeModule_.get(); }
   /// @brief 寿命中のサイズ変更モジュールを取得する
   /// @return サイズモジュール
   SizeOverLifetimeModule* GetSizeOverLifetimeModule() { return sizeOverLifetimeModule_.get(); }
   /// @brief 寿命中の回転変更モジュールを取得する
   /// @return 回転モジュール
   RotationOverLifetimeModule* GetRotationOverLifetimeModule() { return rotationOverLifetimeModule_.get(); }

   /// @brief 寿命中に力を加えるモジュールを取得する
   /// @return 力モジュール
   ForceOverLifetimeModule* GetForceOverLifetimeModule() { return forceOverLifetimeModule_.get(); }
   /// @brief 速度上限モジュールを取得する
   /// @return 速度上限モジュール
   LimitVelocityOverLifetimeModule* GetLimitVelocityModule() { return limitVelocityModule_.get(); }
   /// @brief 擬似ノイズによる揺らぎモジュールを取得する
   /// @return ノイズモジュール
   NoiseModule* GetNoiseModule() { return noiseModule_.get(); }
   /// @brief UV移動・回転・拡縮モジュールを取得する
   /// @return UV変換モジュール
   UVTransformModule* GetUVTransformModule() { return uvTransformModule_.get(); }
   /// @brief テクスチャシート再生モジュールを取得する
   /// @return シートアニメーションモジュール
   TextureSheetAnimationModule* GetTextureSheetAnimationModule() { return textureSheetAnimationModule_.get(); }

   /// @brief ビルボード・ソートなどの描画設定モジュールを取得する
   /// @return 描画モジュール
   RendererModule* GetRendererModule() { return rendererModule_.get(); }

   /// @brief トレイル設定モジュールを取得する
   /// @return トレイル設定モジュール
   TrailModule* GetTrailModule() { return trailModule_.get(); }

   /// @brief パーティクルメッシュ設定モジュールを取得する
   /// @return パーティクルメッシュ設定モジュール
   ParticleMeshModule* GetParticleMeshModule() { return particleMeshModule_.get(); }

   /// @brief マテリアルを取得
   ParticleMaterial* GetMaterial() { return material_.get(); }

   /// @brief テクスチャを設定
   void SetTexture(Texture* texture);

   /// @brief テクスチャ名を設定して適用
   void SetTextureName(const std::string& textureName);

   /// @brief 設定中のテクスチャ名を取得
   const std::string& GetTextureName() const { return textureName_; }

   /// @brief エディタとデバッグ表示に使うシステム名を取得する
   /// @return システム名
   const std::string& GetName() const { return name_; }
   /// @brief エディタとデバッグ表示に使うシステム名を設定する
   /// @param name 新しいシステム名
   void SetName(const std::string& name) { name_ = name; }

   /// @brief エディタのヒエラルキーとインスペクターへ公開するか取得する
   /// @return エディタで編集可能な場合はtrue
   bool IsEditorInspectable() const { return isEditorInspectable_; }
   /// @brief エディタのヒエラルキーとインスペクターへの公開可否を設定する
   /// @param inspectable 公開する場合はtrue
   void SetEditorInspectable(bool inspectable) { isEditorInspectable_ = inspectable; }

   /// @brief ブレンドモードを設定（マテリアルに委譲）
   void SetBlendMode(std::optional<BlendMode> mode) { if (material_) material_->SetBlendMode(mode); }

   /// @brief ブレンドモードを取得（マテリアルに委譲）
   std::optional<BlendMode> GetBlendMode() const { return material_ ? material_->GetBlendMode() : std::nullopt; }

   /// @brief ポストプロセスを適用するか設定
   void SetUsePostProcess(bool use) { usePostProcess_ = use; }

   /// @brief ポストプロセスを適用するか取得
   bool GetUsePostProcess() const { return usePostProcess_; }

   /// @brief サブエミッター設定を取得する
   SubEmitterSettings& GetSubEmitterSettings() { return subEmitterSettings_; }

   /// @brief サブエミッター設定を読み取り専用で取得する
   const SubEmitterSettings& GetSubEmitterSettings() const { return subEmitterSettings_; }

   /// @brief GPUシミュレーションに必要なリソースが利用可能か取得する
   /// @return 必須GPUリソースがすべて作成済みならtrue
   bool CanUseGpuSimulation() const;

   /// @brief 描画直前にCompute Shaderシミュレーションを実行する
   /// @param psoManager Computeパイプラインの取得元
   void DispatchGpuSimulation(PSOManager* psoManager);

   // ============ JSON Serialization ============
   /// @brief パラメータをJSONファイルに保存
   /// @param filePath 保存先のファイルパス
   /// @return 成功したかどうか
   bool SaveToJson(const std::string& filePath) const;

   /// @brief パラメータをJSONファイルから読み込み
   /// @param filePath 読み込み元のファイルパス
   /// @return 成功したかどうか
   bool LoadFromJson(const std::string& filePath);

   /// @brief パラメータをJSON形式で取得
   /// @return JSON形式のパラメータ
   nlohmann::json ToJson() const;

   /// @brief JSON形式のパラメータを設定
   /// @param json JSON形式のパラメータ
   void FromJson(const nlohmann::json& json);

   /// @brief パーティクル描画に使用する現在のメッシュを取得する
   /// @return クアッドまたは生成済み形状メッシュ
   Mesh* GetMesh() const { return quadMesh_.get(); }
   /// @brief CPUシミュレーション用インスタンスSRVを取得する
   /// @return インスタンシングバッファのGPUハンドル
   D3D12_GPU_DESCRIPTOR_HANDLE GetInstancingSrvHandleGPU() const;
   /// @brief パーティクル本体のテクスチャを取得する
   /// @return 設定中のテクスチャ
   Texture* GetTexture() const;

   /// @brief リボン描画に使用するテクスチャを取得する
   /// @return 専用テクスチャ、未設定または無効な場合はパーティクル本体のテクスチャ
   Texture* GetRibbonTexture() const;

   /// @brief 描画パスへ渡す内部マテリアルを取得する
   /// @return パーティクル用マテリアル
   Material* GetMaterialForRenderer() const;
   /// @brief CPU側で生存しているパーティクル数を取得する
   /// @return 生存パーティクル数
   uint32_t GetActiveParticleCount() const { return activeParticleCount_; }

   /// @brief GPUシミュレーションが生成した描画インスタンス数を取得する
   /// @return 描画に使用するGPU出力要素数
   uint32_t GetDrawParticleCount() const;
   /// @brief メッシュパーティクルへ使用するモデルアセットを取得する
   /// @return 設定中のモデル。未設定の場合はnullptr
   ModelAsset* GetModelAsset() const { return modelAsset_; }
   /// @brief メッシュパーティクルへ使用するモデルアセットを設定する
   /// @param model 使用するモデル。nullptrで解除
   void SetModelAsset(ModelAsset* model) { modelAsset_ = model; }

   /// @brief リボン描画用GPU頂点バッファビューを取得する
   const D3D12_VERTEX_BUFFER_VIEW& GetRibbonVertexBufferView() const { return gpuRibbonVertexBufferView_; }

   /// @brief ワールド空間で生成済みのリボン頂点を描画するための単位行列SRVを取得する
   /// @return リボン描画用インスタンシングSRV
   D3D12_GPU_DESCRIPTOR_HANDLE GetRibbonInstancingSrvHandleGPU() const { return instancingSrvHandleGPU_; }

   /// @brief リボン描画用GPUインデックスバッファビューを取得する
   const D3D12_INDEX_BUFFER_VIEW& GetRibbonIndexBufferView() const { return gpuRibbonIndexBufferView_; }

   /// @brief リボン描画用インデックス数を取得する
   uint32_t GetRibbonIndexCount() const { return gpuRibbonIndexCount_; }
   // ========================================================

private:
   /// @brief 親粒子の消滅後に末尾から巻き取るリボン履歴
   struct DetachedRibbon {
	  std::vector<Vector3> points;
	  float width = 0.5f;
	  float age = 0.0f;
	  float retractionDuration = 0.5f;
   };

   // パーティクル管理
   std::vector<Particle> particles_;
   std::deque<DetachedRibbon> detachedRibbons_;
   TrailModule::TrailMode lastTrailMode_ = TrailModule::TrailMode::ParticlePath;
   uint32_t activeParticleCount_ = 0;

   // O(1) フリーリスト（非アクティブパーティクルのインデックスを管理）
   std::stack<uint32_t> freeParticleIndices_;

   // ============ Unity-like Modules ============
   std::unique_ptr<MainModule> mainModule_ = nullptr;
   std::unique_ptr<EmissionModule> emissionModule_ = nullptr;
   std::unique_ptr<ShapeModule> shapeModule_ = nullptr;

   std::unique_ptr<VelocityOverLifetimeModule> velocityOverLifetimeModule_ = nullptr;
   std::unique_ptr<ColorOverLifetimeModule> colorOverLifetimeModule_ = nullptr;
   std::unique_ptr<SizeOverLifetimeModule> sizeOverLifetimeModule_ = nullptr;
   std::unique_ptr<RotationOverLifetimeModule> rotationOverLifetimeModule_ = nullptr;

   std::unique_ptr<ForceOverLifetimeModule> forceOverLifetimeModule_ = nullptr;
   std::unique_ptr<LimitVelocityOverLifetimeModule> limitVelocityModule_ = nullptr;
   std::unique_ptr<NoiseModule> noiseModule_ = nullptr;
   std::unique_ptr<UVTransformModule> uvTransformModule_ = nullptr;
   std::unique_ptr<TextureSheetAnimationModule> textureSheetAnimationModule_ = nullptr;

   std::unique_ptr<RendererModule> rendererModule_ = nullptr;
   std::unique_ptr<TrailModule> trailModule_ = nullptr;
   std::unique_ptr<ParticleMeshModule> particleMeshModule_ = nullptr;
   // ============================================

   std::unique_ptr<ParticleMaterial> material_ = nullptr;

   // GPU インスタンシングリソース
   Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_ = nullptr;
   ParticleForGPU* instancingData_ = nullptr;
   D3D12_CPU_DESCRIPTOR_HANDLE instancingSrvHandleCPU_{};
   D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU_{};
   UINT instancingSrvIndex_ = UINT_MAX;  // 解放用に記録

   enum GpuDescriptorSlot : size_t {
	  kGpuStateUavDescriptor,
	  kGpuStateSrvDescriptor,
	  kGpuAttributesSrvDescriptor,
	  kGpuOutputUavDescriptor,
	  kGpuOutputSrvDescriptor,
	  kGpuMotionUavDescriptor,
	  kGpuSpawnRequestSrvDescriptor,
	  kGpuAliveUavDescriptor,
	  kGpuFreeListUavDescriptor,
	  kGpuFreeCountUavDescriptor,
	  kGpuOwnerMappingUavDescriptor,
	  kGpuOwnerMappingSrvDescriptor,
	  kGpuDescriptorCount
   };

   Microsoft::WRL::ComPtr<ID3D12Resource> gpuStateResource_ = nullptr;
   Microsoft::WRL::ComPtr<ID3D12Resource> gpuMotionResource_ = nullptr;
   Microsoft::WRL::ComPtr<ID3D12Resource> gpuSpawnRequestResource_ = nullptr;
   Microsoft::WRL::ComPtr<ID3D12Resource> gpuAliveResource_ = nullptr;
   Microsoft::WRL::ComPtr<ID3D12Resource> gpuFreeListResource_ = nullptr;
   Microsoft::WRL::ComPtr<ID3D12Resource> gpuFreeCountResource_ = nullptr;
   Microsoft::WRL::ComPtr<ID3D12Resource> gpuOwnerMappingResource_ = nullptr;
   Microsoft::WRL::ComPtr<ID3D12Resource> gpuAttributesResource_ = nullptr;
   Microsoft::WRL::ComPtr<ID3D12Resource> gpuOutputResource_ = nullptr;
   Microsoft::WRL::ComPtr<ID3D12Resource> gpuSettingsResource_ = nullptr;
   // CPUソートは前フレームのGPU位置を非同期Readbackし、描画中のGPU待機を発生させない。
   Microsoft::WRL::ComPtr<ID3D12Resource> gpuStateReadbackResource_ = nullptr;
   GpuSpawnRequest* gpuSpawnRequestData_ = nullptr;
   GpuParticleAttributes* gpuAttributesData_ = nullptr;
   GpuSimulationSettings* gpuSettingsData_ = nullptr;
   GpuParticleState* gpuStateReadbackData_ = nullptr;
   D3D12_GPU_DESCRIPTOR_HANDLE gpuStateUavHandleGPU_{};
   D3D12_GPU_DESCRIPTOR_HANDLE gpuStateSrvHandleGPU_{};
   D3D12_GPU_DESCRIPTOR_HANDLE gpuMotionUavHandleGPU_{};
   D3D12_GPU_DESCRIPTOR_HANDLE gpuSpawnRequestSrvHandleGPU_{};
   D3D12_GPU_DESCRIPTOR_HANDLE gpuAliveUavHandleGPU_{};
   D3D12_GPU_DESCRIPTOR_HANDLE gpuFreeListUavHandleGPU_{};
   D3D12_GPU_DESCRIPTOR_HANDLE gpuFreeCountUavHandleGPU_{};
   D3D12_GPU_DESCRIPTOR_HANDLE gpuOwnerMappingUavHandleGPU_{};
   D3D12_GPU_DESCRIPTOR_HANDLE gpuOwnerMappingSrvHandleGPU_{};
   D3D12_GPU_DESCRIPTOR_HANDLE gpuAttributesSrvHandleGPU_{};
   D3D12_GPU_DESCRIPTOR_HANDLE gpuOutputUavHandleGPU_{};
   D3D12_GPU_DESCRIPTOR_HANDLE gpuOutputSrvHandleGPU_{};
   std::array<UINT, kGpuDescriptorCount> gpuDescriptorIndices_ = [] {
	  std::array<UINT, kGpuDescriptorCount> indices{};
	  indices.fill(UINT_MAX);
	  return indices;
	  }();
   D3D12_RESOURCE_STATES gpuStateResourceState_ = D3D12_RESOURCE_STATE_COMMON;
   D3D12_RESOURCE_STATES gpuOwnerMappingResourceState_ = D3D12_RESOURCE_STATE_COMMON;
   D3D12_RESOURCE_STATES gpuOutputResourceState_ = D3D12_RESOURCE_STATE_COMMON;
   std::vector<uint32_t> renderParticleIndices_;
   std::vector<uint32_t> gpuStateIndexByCpuParticle_;
   uint32_t gpuRenderParticleCount_ = 0;
   uint32_t gpuPendingSpawnRequestCount_ = 0;
   bool gpuStateReadbackAvailable_ = false;
   bool gpuNeedsInitialize_ = true;
   uint32_t gpuInitializationStartIndex_ = 0;
   float gpuDeltaTime_ = 0.0f;

   Microsoft::WRL::ComPtr<ID3D12Resource> gpuRibbonInputResource_ = nullptr;
   Microsoft::WRL::ComPtr<ID3D12Resource> gpuRibbonSettingsResource_ = nullptr;
   Microsoft::WRL::ComPtr<ID3D12Resource> gpuRibbonVertexResource_ = nullptr;
   Microsoft::WRL::ComPtr<ID3D12Resource> gpuRibbonIndexResource_ = nullptr;
   GpuRibbonSegment* gpuRibbonInputData_ = nullptr;
   GpuSimulationSettings* gpuRibbonSettingsData_ = nullptr;
   D3D12_GPU_DESCRIPTOR_HANDLE gpuRibbonInputSrvHandleGPU_{};
   D3D12_GPU_DESCRIPTOR_HANDLE gpuRibbonVertexUavHandleGPU_{};
   D3D12_GPU_DESCRIPTOR_HANDLE gpuRibbonIndexUavHandleGPU_{};
   std::array<UINT, 3> gpuRibbonDescriptorIndices_{ UINT_MAX, UINT_MAX, UINT_MAX };
   D3D12_VERTEX_BUFFER_VIEW gpuRibbonVertexBufferView_{};
   D3D12_INDEX_BUFFER_VIEW gpuRibbonIndexBufferView_{};
   D3D12_RESOURCE_STATES gpuRibbonVertexState_ = D3D12_RESOURCE_STATE_COMMON;
   D3D12_RESOURCE_STATES gpuRibbonIndexState_ = D3D12_RESOURCE_STATE_COMMON;
   uint32_t gpuRibbonSegmentCapacity_ = 0;
   uint32_t gpuRibbonSegmentCount_ = 0;
   uint32_t gpuRibbonIndexCount_ = 0;

   // レンダリング設定
   ModelAsset* modelAsset_ = nullptr;
   Texture* texture_ = nullptr;
   std::string textureName_;

   // 再生制御
   bool isPlaying_ = false;
   bool isPaused_ = false;

   bool isEditorInspectable_ = true;
   bool usePostProcess_ = false;
   float emissionTimer_ = 0.0f;
   float emissionAccumulator_ = 0.0f;
   float emissionDistanceAccumulator_ = 0.0f;
   Vector3 previousEmitterPosition_{ 0.0f, 0.0f, 0.0f };
   bool hasPreviousEmitterPosition_ = false;
   float systemTime_ = 0.0f;

   std::unique_ptr<Mesh> quadMesh_ = nullptr;
   bool isCreated_ = false;
   std::string name_;

   static std::vector<ParticleSystem*> sRegisteredParticleSystems_;
   struct PendingSubEmitterEvent {
	  std::string effectPath;
	  Vector3 position;
   };
   static std::vector<PendingSubEmitterEvent> sPendingSubEmitterEvents_;
   static std::vector<std::shared_ptr<ParticleSystem>> sRuntimeSubEmitters_;

   SubEmitterSettings subEmitterSettings_;
   uint32_t subEmitterEventsThisFrame_ = 0;

private:
   /// @brief クワッドメッシュを作成（ビルボード用）
   void CreateQuadMesh();

   /// @brief GPUシミュレーション用バッファとビューを作成する
   void CreateGpuSimulationResources();

   /// @brief 実行中に増やされた最大粒子数をCPU所有スロットとGPU FreeListへ反映する
   void EnsureParticlePoolCapacity();

   /// @brief 指定CPU粒子の生成または状態上書きをEmitter CS要求キューへ登録する
   /// @param particleIndex CPU側の所有粒子番号
   /// @param overwriteExisting trueなら既存GPU状態を上書きし、falseならFreeListから新規確保する
   void QueueGpuParticleCommand(uint32_t particleIndex, bool overwriteExisting);

   /// @brief マテリアルとRenderer設定から実際のCPU描画ソート方式を解決する
   RendererModule::SortMode ResolveSortMode() const;

   /// @brief 必要区間数に応じてリボンCompute資源を確保する
   void EnsureGpuRibbonResources(uint32_t requiredSegmentCount);

   /// @brief リボン頂点・インデックス生成Compute Shaderを実行する
   void DispatchGpuRibbon(PSOManager* psoManager);

   /// @brief サブエミッター生成を遅延イベントキューへ登録する
   void QueueSubEmitter(const std::string& effectPath, const Vector3& position);

   /// @brief ParticleMeshModule の形状設定に基づいてパーティクルメッシュを再構築する
   void RebuildParticleMesh();

   /// @brief 粒子履歴からカメラ向きのリボンメッシュを更新する
   void BuildRibbonMesh(Camera* camera);

   /// @brief 選択中のトレイル方式に応じて粒子のリボン点を更新する
   /// @param particle 更新対象の粒子
   void UpdateTrailPoints(Particle& particle);

   /// @brief トレイル方式の変更を検出し、既存点列を新しい方式で初期化する
   void SynchronizeTrailMode();

   /// @brief 親粒子からリボン履歴を切り離して巻き取り対象へ移す
   void DetachRibbon(Particle& particle);

   /// @brief 切り離されたリボンの巻き取り時間を更新して期限切れを破棄する
   void UpdateDetachedRibbons(float deltaTime);

   /// @brief 粒子を放出
   void EmitParticle();

   /// @brief UV変換とフリップブックのセル変換を合成する
   Matrix4x4 BuildParticleUVTransform(const Particle& particle) const;
};
}
