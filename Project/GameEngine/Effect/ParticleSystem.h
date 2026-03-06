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
#include "Core/Graphics/Mesh.h"
#include "Core/Graphics/Texture.h"
#include "Core/Graphics/Material.h"
#include "Object/Model/ModelAsset.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <vector>
#include <wrl.h>

namespace GameEngine {
class GraphicsDevice;
class Camera;

/// @brief Unity ライクなパーティクルシステム
class ParticleSystem {
public:
   static constexpr uint32_t kMaxParticles = 2048;

   /// @brief GPU送信用パーティクルデータ
   struct ParticleForGPU {
	  Matrix4x4 wvp;
	  Matrix4x4 world;
	  Vector4 color;
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

   /// @brief 再生中か判定
   bool IsPlaying() const { return isPlaying_; }

   // ============ Module Access ============
   MainModule* GetMainModule() { return mainModule_.get(); }
   EmissionModule* GetEmissionModule() { return emissionModule_.get(); }
   ShapeModule* GetShapeModule() { return shapeModule_.get(); }

   VelocityOverLifetimeModule* GetVelocityOverLifetimeModule() { return velocityOverLifetimeModule_.get(); }
   ColorOverLifetimeModule* GetColorOverLifetimeModule() { return colorOverLifetimeModule_.get(); }
   SizeOverLifetimeModule* GetSizeOverLifetimeModule() { return sizeOverLifetimeModule_.get(); }
   RotationOverLifetimeModule* GetRotationOverLifetimeModule() { return rotationOverLifetimeModule_.get(); }

   ForceOverLifetimeModule* GetForceOverLifetimeModule() { return forceOverLifetimeModule_.get(); }
   LimitVelocityOverLifetimeModule* GetLimitVelocityModule() { return limitVelocityModule_.get(); }
   NoiseModule* GetNoiseModule() { return noiseModule_.get(); }

   RendererModule* GetRendererModule() { return rendererModule_.get(); }

   /// @brief マテリアルを取得
   ParticleMaterial* GetMaterial() { return material_.get(); }

   /// @brief テクスチャを設定
   void SetTexture(Texture* texture);

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

   // ============ Renderer 用公開インターフェース ============
   Mesh* GetMesh() const { return quadMesh_.get(); }
   D3D12_GPU_DESCRIPTOR_HANDLE GetInstancingSrvHandleGPU() const { return instancingSrvHandleGPU_; }
   Texture* GetTexture() const;
   Material* GetMaterialForRenderer() const;
   uint32_t GetActiveParticleCount() const { return activeParticleCount_; }
   ModelAsset* GetModelAsset() const { return modelAsset_; }
   void SetModelAsset(ModelAsset* model) { modelAsset_ = model; }
   // ========================================================

private:
   // パーティクル管理
   std::vector<Particle> particles_;
   uint32_t activeParticleCount_ = 0;

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

   std::unique_ptr<RendererModule> rendererModule_ = nullptr;
   // ============================================

   std::unique_ptr<ParticleMaterial> material_ = nullptr;

   // GPU インスタンシングリソース
   Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_ = nullptr;
   ParticleForGPU* instancingData_ = nullptr;
   D3D12_CPU_DESCRIPTOR_HANDLE instancingSrvHandleCPU_{};
   D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU_{};

   // レンダリング設定
   ModelAsset* modelAsset_ = nullptr;
   Texture* texture_ = nullptr;

   // 再生制御
   bool isPlaying_ = false;
   bool isPaused_ = false;
   float emissionTimer_ = 0.0f;
   float emissionAccumulator_ = 0.0f;
   float systemTime_ = 0.0f;

   std::unique_ptr<Mesh> quadMesh_ = nullptr;
   bool isCreated_ = false;

private:
   /// @brief クワッドメッシュを作成（ビルボード用）
   void CreateQuadMesh();

   /// @brief 粒子を放出
   void EmitParticle();

   /// @brief 非アクティブな粒子を探して返す
   Particle* FindInactiveParticle();

   /// @brief モジュールの更新を適用
   void ApplyModules(Particle& particle, float deltaTime);
};
}