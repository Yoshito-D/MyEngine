#pragma once

#include "GameEngine/Math/VectorMath.h"
#include "GameEngine/Math/MathUtils.h"
#include "GameEngine/Effects/Particles/Particle.h"
#include "GameEngine/Effects/Particles/ParticleMaterial.h"
#include "GameEngine/Effects/Particles/Modules/MainModule.h"
#include "GameEngine/Effects/Particles/Modules/EmissionModule.h"
#include "GameEngine/Effects/Particles/Modules/ShapeModule.h"
#include "GameEngine/Effects/Particles/Modules/VelocityOverLifetimeModule.h"
#include "GameEngine/Effects/Particles/Modules/LimitVelocityOverLifetimeModule.h"
#include "GameEngine/Effects/Particles/Modules/ForceOverLifetimeModule.h"
#include "GameEngine/Effects/Particles/Modules/ColorOverLifetimeModule.h"
#include "GameEngine/Effects/Particles/Modules/SizeOverLifetimeModule.h"
#include "GameEngine/Effects/Particles/Modules/RotationOverLifetimeModule.h"
#include "GameEngine/Effects/Particles/Modules/UVTransformModule.h"
#include "GameEngine/Effects/Particles/Modules/TextureSheetAnimationModule.h"
#include "GameEngine/Effects/Particles/Modules/NoiseModule.h"
#include "GameEngine/Effects/Particles/Modules/RendererModule.h"
#include "GameEngine/Effects/Particles/Modules/TrailModule.h"
#include "GameEngine/Effects/Particles/Modules/ParticleMeshModule.h"
#include "GameEngine/Graphics/Resources/Mesh.h"
#include "GameEngine/Graphics/Resources/Texture.h"
#include "GameEngine/Graphics/Resources/Material.h"
#include "GameEngine/Assets/Model/ModelAsset.h"
#include <nlohmann/json.hpp>
#include <optional>
#include <stack>
#include <type_traits>
#include <memory>
#include <vector>

namespace GameEngine {
class SceneWorld;
class GraphicsDevice;
class Camera;
class PSOManager;
class ParticleGpuSimulation;
class ParticleTrailRuntime;

/// @brief 粒子の設定・再生・CPU属性を管理し、GPU処理とトレイルを協調させるFacade。
class ParticleSystem {
public:
   static constexpr uint32_t kMaxParticles = 4096; ///< 粒子用GPUバッファとCPU管理配列で扱える粒子数の上限。

   /// @brief 親粒子イベントから生成する子エフェクト設定
   struct SubEmitterSettings {
      bool enabled = false;                           ///< この機能を実行するかどうか。
      std::string spawnOnDeathPath;                   ///< 粒子消滅時に生成する子エフェクトの設定ファイル。
      std::string spawnOnUpdatePath;                  ///< 一定間隔で生成する子エフェクトの設定ファイル。
      std::string spawnOnCollisionPath;               ///< 衝突時に生成する子エフェクトの設定ファイル。
      float updateInterval = 0.1f;                    ///< 更新イベントを繰り返す間隔（秒）。
      uint32_t maxEventsPerFrame = 32;                ///< 1フレームに生成するサブエミッターイベント数の上限。
      Vector3 collisionPlaneNormal{0.0f, 1.0f, 0.0f}; ///< 粒子衝突に使う平面の法線方向。
      float collisionPlaneDistance = 0.0f;            ///< 衝突平面の方程式normal・position=distanceの定数項。
      float collisionRestitution = 0.0f;              ///< 衝突時の法線方向速度へ適用する反発係数。
   };

   /// @brief 自動更新・描画対象として登録された全システムを取得する
   /// @return 生存中の登録システム一覧
   static const std::vector<ParticleSystem*>& GetRegisteredParticleSystems();

   /// @brief 指定システムを自動更新・描画一覧から外す
   /// @param particleSystem 解除対象
   static void UnregisterParticleSystem(ParticleSystem* particleSystem);

   /// @brief 自動更新・描画レジストリを空にする
   static void ClearRegisteredParticleSystems();

   /// @brief パーティクルを更新元のWorldへ関連付ける。
   void SetWorld(SceneWorld* world) { world_ = world; }

   /// @brief 更新元のWorldを取得する。描画レジストリは所有権を持たない。
   SceneWorld* GetWorld() const { return world_; }

   /// @brief GPU送信用パーティクルデータ
   struct ParticleForGPU {
      Matrix4x4 wvp;         ///< ローカル頂点をクリップ空間へ移すワールド・ビュー・投影合成行列。
      Matrix4x4 world;       ///< ローカル頂点をワールド座標系へ移す変換行列。
      Matrix4x4 uvTransform; ///< UV変換行列。
      Vector4 color;         ///< 表示色のRGBA成分。
      Vector4 customData;    ///< シェーダー連携用（年齢、乱数、カメラフェード、速度）。
   };

   /// @brief Compute Shader が更新する最小シミュレーション状態
   struct GpuParticleState {
      Vector4 positionAndActive;              ///< xyzは粒子の位置、wは生存フラグ。
      Vector4 velocityAndLifetime;            ///< xyzは粒子速度、wは残り寿命（秒）。
      uint32_t ownerParticleIndex = UINT_MAX; ///< 対応するCPU粒子番号。UINT_MAXは未割り当て。
      float age = 0.0f;                       ///< 生成または処理開始からの経過時間（秒）。
      float initialLifetime = 0.0f;           ///< 生成時に決めた粒子寿命（秒）。
      uint32_t padding = 0;                   ///< GPU転送データの配置境界を揃える未使用領域。
   };

   /// @brief Update CSが粒子ごとに参照する永続的な運動パラメータ
   struct GpuParticleMotion {
      Vector4 forceAndDrag;              ///< xyzは加速度（m/s²）、wは抵抗係数（1/s）。
      Vector4 accelerationAndGrowthRate; ///< xyzは加速度（m/s²）、wは速度の指数増幅率（1/s）。
      Vector4 limitAndGravity;           ///< xは速度上限（m/s）、yは減衰係数（1/s）、zは重力倍率。
      Vector4 noiseParams;               ///< xは加速度振幅（m/s²）、yは空間周波数（rad/m）、zは位相速度（rad/s）。
   };

   /// @brief CPUで初期化した粒子固有値をGPUシミュレーションへ渡す属性
   struct GpuParticleAttributes {
      Matrix4x4 uvTransform;      ///< UV変換行列。
      Vector4 color;              ///< 表示色のRGBA成分。
      Vector4 sizeAndRotation;    ///< xyzは粒子の各軸サイズ、wはZ軸回転角（ラジアン）。
      Vector4 rotationQuaternion; ///< 姿勢を表す回転クォータニオン。
      Vector4 customData;         ///< シェーダー連携用（年齢、乱数、カメラフェード、速度）。
      uint32_t stateIndex = 0;    // 圧縮済み描画属性から永続GPU状態を引くための元粒子番号
      uint32_t padding[3]{};      ///< GPU転送データの配置境界を揃える未使用領域。
   };

   /// @brief Emitter CSへ渡す生成または状態上書き要求
   struct GpuSpawnRequest {
      GpuParticleState state;   ///< 生成する粒子の位置・速度・寿命・CPU所有番号。
      GpuParticleMotion motion; ///< GPUで積分する外力・速度倍率・制限・ノイズの設定。
      uint32_t operation = 0;   // 0: FreeListから生成、1: 既存所有粒子を上書き
      uint32_t padding[3]{};    ///< GPU転送データの配置境界を揃える未使用領域。
   };

   /// @brief GPUシミュレーションのフレーム定数
   struct GpuSimulationSettings {
      Matrix4x4 viewProjection;              ///< ワールド座標をクリップ空間へ移すビュー・投影合成行列。
      Vector4 cameraPosition;                ///< 描画に使うカメラのワールド位置。
      Vector4 cameraRight;                   ///< カメラのワールド右方向。
      Vector4 cameraUp;                      ///< カメラのワールド上方向。
      Vector4 cameraForward;                 ///< xyzはカメラの前方、wはGPU粒子更新の時間刻み（秒）。
      Vector4 renderParams;                  ///< xはビルボード種類、yは速度倍率、zは長さ倍率、wは速度ストレッチ有効値。
      Vector4 cameraFadeParams;              ///< xは距離フェードの有効値、yは開始距離、zは終了距離。
      Vector4 attractorPosition;             ///< 粒子を引き寄せる中心のワールド位置。
      Vector4 attractorParams;               ///< xは吸引強度、yは有効半径、zは減衰指数、wは吸引の有効値。
      Vector4 simulationOriginAndLocal;      ///< xyzはエミッター原点、wはローカル空間で評価するかのフラグ。
      Vector4 simulationRotation;            ///< ローカル設定をワールド方向へ回転するエミッターのクォータニオン。
      uint32_t particleCount = 0;            ///< 描画出力へ詰める有効粒子数。
      uint32_t particleCapacity = 0;         ///< GPU粒子プールに確保したスロット数。
      uint32_t spawnRequestCount = 0;        ///< 今回のDispatchで処理する生成・衝突補正要求数。
      uint32_t initializationStartIndex = 0; ///< 初期化するGPU粒子スロットの先頭番号。
   };

   /// @brief リボン1区間をCompute Shaderへ渡す入力
   struct GpuRibbonSegment {
      Vector4 startAndWidth;    ///< xyzはリボン線分の始点、wは始点側の幅。
      Vector4 endAndWidth;      ///< xyzはリボン線分の終点、wは終点側の幅。
      Vector4 startTangentAndV; ///< xyzは始点の接線、wは長さ方向のテクスチャ座標。
      Vector4 endTangentAndV;   ///< xyzは終点の接線、wは長さ方向のテクスチャ座標。
      Vector4 alphaAndPadding;  ///< xはリボンの透明度、残りはGPU配置調整用の未使用領域。
   };

   /// @brief シミュレーション空間
   enum class SimulationSpace {
      World, // ワールド空間
      Local  // ローカル空間
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
   /// @return 内部の変更権限を持たないメインモジュール
   MainModule DescribeMainModule() const { return *mainModule_; }

   /// @brief 編集用コピーの設定だけを検証・適用する。再生状態やGPU更新順は維持する。
   bool ConfigureModule(const MainModule& configuration);

   /// @brief 時間・距離・バースト放出を管理するモジュールを取得する
   /// @return 内部の変更権限を持たない放出モジュール
   EmissionModule DescribeEmissionModule() const { return *emissionModule_; }

   /// @brief 編集用コピーの設定だけを検証・適用する。再生状態やGPU更新順は維持する。
   bool ConfigureModule(const EmissionModule& configuration);

   /// @brief 放出位置と初期方向を管理する形状モジュールを取得する
   /// @return 内部の変更権限を持たない形状モジュール
   ShapeModule DescribeShapeModule() const { return *shapeModule_; }

   /// @brief 編集用コピーの設定だけを検証・適用する。再生状態やGPU更新順は維持する。
   bool ConfigureModule(const ShapeModule& configuration);

   /// @brief 寿命中の速度変更モジュールを取得する
   /// @return 速度モジュール
   VelocityOverLifetimeModule DescribeVelocityOverLifetimeModule() const { return *velocityOverLifetimeModule_; }

   /// @brief 編集用コピーの設定だけを検証・適用する。再生状態やGPU更新順は維持する。
   bool ConfigureModule(const VelocityOverLifetimeModule& configuration);

   /// @brief 寿命中の色変更モジュールを取得する
   /// @return 色モジュール
   ColorOverLifetimeModule DescribeColorOverLifetimeModule() const { return *colorOverLifetimeModule_; }

   /// @brief 編集用コピーの設定だけを検証・適用する。再生状態やGPU更新順は維持する。
   bool ConfigureModule(const ColorOverLifetimeModule& configuration);

   /// @brief 寿命中のサイズ変更モジュールを取得する
   /// @return サイズモジュール
   SizeOverLifetimeModule DescribeSizeOverLifetimeModule() const { return *sizeOverLifetimeModule_; }

   /// @brief 編集用コピーの設定だけを検証・適用する。再生状態やGPU更新順は維持する。
   bool ConfigureModule(const SizeOverLifetimeModule& configuration);

   /// @brief 寿命中の回転変更モジュールを取得する
   /// @return 回転モジュール
   RotationOverLifetimeModule DescribeRotationOverLifetimeModule() const { return *rotationOverLifetimeModule_; }

   /// @brief 編集用コピーの設定だけを検証・適用する。再生状態やGPU更新順は維持する。
   bool ConfigureModule(const RotationOverLifetimeModule& configuration);

   /// @brief 寿命中に力を加えるモジュールを取得する
   /// @return 力モジュール
   ForceOverLifetimeModule DescribeForceOverLifetimeModule() const { return *forceOverLifetimeModule_; }

   /// @brief 編集用コピーの設定だけを検証・適用する。再生状態やGPU更新順は維持する。
   bool ConfigureModule(const ForceOverLifetimeModule& configuration);

   /// @brief 速度上限モジュールを取得する
   /// @return 速度上限モジュール
   LimitVelocityOverLifetimeModule DescribeLimitVelocityModule() const { return *limitVelocityModule_; }

   /// @brief 編集用コピーの設定だけを検証・適用する。再生状態やGPU更新順は維持する。
   bool ConfigureModule(const LimitVelocityOverLifetimeModule& configuration);

   /// @brief 擬似ノイズによる揺らぎモジュールを取得する
   /// @return ノイズモジュール
   NoiseModule DescribeNoiseModule() const { return *noiseModule_; }

   /// @brief 編集用コピーの設定だけを検証・適用する。再生状態やGPU更新順は維持する。
   bool ConfigureModule(const NoiseModule& configuration);

   /// @brief UV移動・回転・拡縮モジュールを取得する
   /// @return UV変換モジュール
   UVTransformModule DescribeUVTransformModule() const { return *uvTransformModule_; }

   /// @brief 編集用コピーの設定だけを検証・適用する。再生状態やGPU更新順は維持する。
   bool ConfigureModule(const UVTransformModule& configuration);

   /// @brief テクスチャシート再生モジュールを取得する
   /// @return シートアニメーションモジュール
   TextureSheetAnimationModule DescribeTextureSheetAnimationModule() const { return *textureSheetAnimationModule_; }

   /// @brief 編集用コピーの設定だけを検証・適用する。再生状態やGPU更新順は維持する。
   bool ConfigureModule(const TextureSheetAnimationModule& configuration);

   /// @brief ビルボード・ソートなどの描画設定モジュールを取得する
   /// @return 描画モジュール
   RendererModule DescribeRendererModule() const { return *rendererModule_; }

   /// @brief 編集用コピーの設定だけを検証・適用する。再生状態やGPU更新順は維持する。
   bool ConfigureModule(const RendererModule& configuration);

   /// @brief トレイル設定モジュールを取得する
   /// @return トレイル設定モジュール
   TrailModule DescribeTrailModule() const { return *trailModule_; }

   /// @brief 編集用コピーの設定だけを検証・適用する。再生状態やGPU更新順は維持する。
   bool ConfigureModule(const TrailModule& configuration);

   /// @brief パーティクルメッシュ設定モジュールを取得する
   /// @return パーティクルメッシュ設定モジュール
   ParticleMeshModule DescribeParticleMeshModule() const { return *particleMeshModule_; }

   /// @brief 編集用コピーの設定だけを検証・適用する。再生状態やGPU更新順は維持する。
   bool ConfigureModule(const ParticleMeshModule& configuration);

   /// @brief 放出元の姿勢を有限値として一括適用する。不正値では以前の姿勢を維持する。
   bool ApplyEmitterPose(const Transform& pose);

   /// @brief スキンメッシュ放出で参照するモデルと現在姿勢をまとめて接続する。
   void BindEmissionMesh(const ModelAsset* model, const SkinCluster* skinCluster);

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
   void SetBlendMode(std::optional<BlendMode> mode);

   /// @brief ブレンドモードを取得（マテリアルに委譲）
   std::optional<BlendMode> GetBlendMode() const { return material_ ? material_->GetBlendMode() : std::nullopt; }

   /// @brief ポストプロセスを適用するか設定
   void SetUsePostProcess(bool use) { usePostProcess_ = use; }

   /// @brief ポストプロセスを適用するか取得
   bool GetUsePostProcess() const { return usePostProcess_; }

   /// @brief 設定を検証し、衝突法線を正規化して一括適用する。
   /// @param settings 間隔は0.001秒以上、イベント上限は1～1024、反発係数は0～1。
   /// @return 不正な値がある場合はfalseを返し、現在の設定を保持する。
   bool ConfigureSubEmitters(const SubEmitterSettings& settings);

   /// @brief サブエミッター設定を読み取り専用で取得する
   const SubEmitterSettings& GetSubEmitterSettings() const { return subEmitterSettings_; }

   /// @brief GPUシミュレーションに必要なリソースが利用可能か取得する
   /// @return 必須GPUリソースがすべて作成済みならtrue
   bool CanUseGpuSimulation() const;

   /// @brief 描画直前にCompute Shaderシミュレーションを実行する
   /// @param psoManager Computeパイプラインの取得元
   void DispatchGpuSimulation(const PSOManager* psoManager);

   // ============ JSONの保存・復元 ============

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
   const Mesh* GetMesh() const { return quadMesh_.get(); }

   /// @brief CPUシミュレーション用インスタンスSRVを取得する
   /// @return インスタンシングバッファのGPUハンドル
   D3D12_GPU_DESCRIPTOR_HANDLE GetInstancingSrvHandleGPU() const;

   /// @brief パーティクル本体のテクスチャを取得する
   /// @return 設定中のテクスチャ
   Texture* GetTexture() const;

   /// @brief リボン描画に使用するテクスチャを取得する
   /// @return 専用テクスチャ、未設定または無効な場合はパーティクル本体のテクスチャ
   Texture* GetRibbonTexture() const;

   /// @brief CPU側で生存しているパーティクル数を取得する
   /// @return 生存パーティクル数
   uint32_t GetActiveParticleCount() const { return activeParticleCount_; }

   /// @brief GPUシミュレーションが生成した描画インスタンス数を取得する
   /// @return 描画に使用するGPU出力要素数
   uint32_t GetDrawParticleCount() const;

   /// @brief メッシュパーティクルへ使用するモデルアセットを取得する
   /// @return 設定中のモデル。未設定の場合はnullptr
   const ModelAsset* GetModelAsset() const { return modelAsset_; }

   /// @brief メッシュパーティクルへ使用するモデルアセットを設定する
   /// @param model 使用するモデル。nullptrで解除
   void SetModelAsset(const ModelAsset* model) { modelAsset_ = model; }

   /// @brief リボン描画用GPU頂点バッファビューを取得する
   const D3D12_VERTEX_BUFFER_VIEW& GetRibbonVertexBufferView() const;

   /// @brief ワールド空間で生成済みのリボン頂点を描画するための単位行列SRVを取得する
   /// @return リボン描画用インスタンシングSRV
   D3D12_GPU_DESCRIPTOR_HANDLE GetRibbonInstancingSrvHandleGPU() const;

   /// @brief リボン描画用GPUインデックスバッファビューを取得する
   const D3D12_INDEX_BUFFER_VIEW& GetRibbonIndexBufferView() const;

   /// @brief リボン描画用インデックス数を取得する
   uint32_t GetRibbonIndexCount() const;

   // ========================================================

private:
   // パーティクル管理
   std::vector<Particle> particles_;  ///< CPU側で管理する粒子ごとの寿命・視覚属性・履歴。
   uint32_t activeParticleCount_ = 0; ///< CPU側で生存しているパーティクル数。

   // O(1) フリーリスト（非アクティブパーティクルのインデックスを管理）
   std::stack<uint32_t> freeParticleIndices_; ///< 未使用のCPU粒子番号をO(1)で取り出すスタック。

   // ============ パーティクルの各種モジュール ============
   static bool HasFiniteSettings(const nlohmann::json& settings);

   template <class Module> bool ApplyModuleConfiguration(Module& target, const Module& configuration);

   std::unique_ptr<MainModule> mainModule_ = nullptr; ///< 寿命・初速・サイズ・再生方式を保持する所有モジュール。
   /// @brief 時間・移動距離・バーストの放出設定を保持する所有モジュール。
   std::unique_ptr<EmissionModule> emissionModule_ = nullptr;
   std::unique_ptr<ShapeModule> shapeModule_ = nullptr; ///< 放出形状とエミッター姿勢を保持する所有モジュール。

   /// @brief 粒子の速度加算と速度倍率を保持する所有モジュール。
   std::unique_ptr<VelocityOverLifetimeModule> velocityOverLifetimeModule_ = nullptr;
   /// @brief 寿命に応じた色変化を保持する所有モジュール。
   std::unique_ptr<ColorOverLifetimeModule> colorOverLifetimeModule_ = nullptr;
   /// @brief 寿命に応じたサイズ変化を保持する所有モジュール。
   std::unique_ptr<SizeOverLifetimeModule> sizeOverLifetimeModule_ = nullptr;
   /// @brief 寿命に応じた回転速度を保持する所有モジュール。
   std::unique_ptr<RotationOverLifetimeModule> rotationOverLifetimeModule_ = nullptr;

   /// @brief 外力・抵抗・吸引設定を保持する所有モジュール。
   std::unique_ptr<ForceOverLifetimeModule> forceOverLifetimeModule_ = nullptr;
   /// @brief 速度上限と減衰設定を保持する所有モジュール。
   std::unique_ptr<LimitVelocityOverLifetimeModule> limitVelocityModule_ = nullptr;
   std::unique_ptr<NoiseModule> noiseModule_ = nullptr; ///< 粒子運動へ加えるノイズ設定を保持する所有モジュール。
   /// @brief 粒子ごとのUV変換設定を保持する所有モジュール。
   std::unique_ptr<UVTransformModule> uvTransformModule_ = nullptr;
   /// @brief 分割テクスチャのフレーム再生設定を保持する所有モジュール。
   std::unique_ptr<TextureSheetAnimationModule> textureSheetAnimationModule_ = nullptr;

   /// @brief ビルボード・ソート・距離フェードの描画設定を保持する所有モジュール。
   std::unique_ptr<RendererModule> rendererModule_ = nullptr;
   /// @brief 粒子履歴から生成するトレイル設定を保持する所有モジュール。
   std::unique_ptr<TrailModule> trailModule_ = nullptr;
   /// @brief 粒子描画に使う形状設定を保持する所有モジュール。
   std::unique_ptr<ParticleMeshModule> particleMeshModule_ = nullptr;

   // ============================================

   /// @brief 粒子描画の色・輝度・テクスチャ効果を保持する所有マテリアル。
   std::unique_ptr<ParticleMaterial> material_ = nullptr;

   /// @brief GPU粒子プール・生成要求・Compute同期を管理する所有実行系。
   std::unique_ptr<ParticleGpuSimulation> gpu_;
   /// @brief 履歴・巻き取りとリボン生成資源を管理する所有実行系。
   std::unique_ptr<ParticleTrailRuntime> trails_;
   std::vector<uint32_t> renderParticleIndices_;      ///< 描画順にソートしたCPU粒子番号。
   std::vector<uint32_t> gpuStateIndexByCpuParticle_; ///< CPU粒子番号から前フレームのGPUスロット番号への対応表。
   SceneWorld* world_ = nullptr;                      ///< 更新と子エフェクト生成を行う所属ワールドへの非所有参照。

   // レンダリング設定
   const ModelAsset* modelAsset_ = nullptr; ///< メッシュパーティクルへ使用するモデルアセット。
   Texture* texture_ = nullptr;             ///< 描画に使用するテクスチャへの非所有参照。
   std::string textureName_;                ///< 設定中のテクスチャ名。

   // 再生制御
   bool isPlaying_ = false; ///< 再生中かどうか。
   bool isPaused_ = false;  ///< 時間経過による更新を一時停止しているかどうか。

   bool isEditorInspectable_ = true;          ///< エディタのヒエラルキーとインスペクターへ公開するかどうかする。
   bool usePostProcess_ = false;              ///< ポストプロセスを適用するかどうか。
   float emissionTimer_ = 0.0f;               ///< 時間ベースの放出処理に使う経過時間（秒）。
   float emissionAccumulator_ = 0.0f;         ///< 時間ベースの放出で次フレームへ繰り越す粒子数の端数。
   float emissionDistanceAccumulator_ = 0.0f; ///< 移動距離ベースの放出で繰り越す粒子数の端数。
   Vector3 previousEmitterPosition_{0.0f, 0.0f, 0.0f}; ///< 移動距離を計算するための前回のエミッター位置。
   bool hasPreviousEmitterPosition_ = false;           ///< 移動距離の比較元となるエミッター位置を取得済みかどうか。
   float systemTime_ = 0.0f;                           ///< 現在の粒子再生を開始してからの経過時間（秒）。

   std::unique_ptr<Mesh> quadMesh_ = nullptr; ///< ビルボードなどの粒子描画に使う所有メッシュ。
   bool isCreated_ = false;                   ///< 粒子描画用メッシュを生成済みかどうか。
   std::string name_;                         ///< エディタとデバッグ表示に使うシステム名。

   /// @brief 描画候補として登録されたParticleSystemの非所有一覧。
   static std::vector<ParticleSystem*> sRegisteredParticleSystems_;

   SubEmitterSettings subEmitterSettings_;  ///< サブエミッター設定。
   uint32_t subEmitterEventsThisFrame_ = 0; ///< このフレームに受理したサブエミッターイベント数。

private:
   /// @brief クワッドメッシュを作成（ビルボード用）
   void CreateQuadMesh();

   /// @brief 実行中に増やされた最大粒子数をCPU所有スロットとGPU FreeListへ反映する
   void EnsureParticlePoolCapacity();

   /// @brief 指定CPU粒子の生成または状態上書きをEmitter CS要求キューへ登録する
   /// @param particleIndex CPU側の所有粒子番号
   /// @param overwriteExisting trueなら既存GPU状態を上書きし、falseならFreeListから新規確保する
   void QueueGpuParticleCommand(uint32_t particleIndex, bool overwriteExisting);

   /// @brief マテリアルとRenderer設定から実際のCPU描画ソート方式を解決する
   RendererModule::SortMode ResolveSortMode() const;

   /// @brief サブエミッター生成を遅延イベントキューへ登録する
   void QueueSubEmitter(const std::string& effectPath, const Vector3& position);

   /// @brief ParticleMeshModule の形状設定に基づいてパーティクルメッシュを再構築する
   void RebuildParticleMesh();

   /// @brief 粒子を放出
   void EmitParticle();

   /// @brief UV変換とフリップブックのセル変換を合成する
   Matrix4x4 BuildParticleUVTransform(const Particle& particle) const;
};
}
