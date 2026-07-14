#pragma once
#include "Utility/VectorMath.h"
#include "Utility/MathUtils.h"
#include "Core/Graphics/IMaterialData.h"
#include "Core/Graphics/PipelineState.h"
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include <optional>

namespace GameEngine {
class Texture;
class GraphicsDevice;

/// @brief パーティクル用マテリアル
class ParticleMaterial : public IMaterialData {
public:
   /// @brief マテリアルデータ（GPU送信用）
   struct MaterialData {
	  Vector4 color;              // 基本カラー
	  Matrix4x4 uvTransform;      // UV変換行列
	  Vector4 renderingParams;    // x: 輝度、y: アルファ閾値、z: トゥーン階調数
	  Vector4 effectParams;       // x: ソフト有効、y: 距離、z: 歪みピクセル、w: 歪み混合率
	  Vector4 sceneParams;        // x: 幅、y: 高さ、z: near、w: far
	  Vector4 projectionParams;   // x: 平行投影フラグ、y: 歪みにRGフローマップを使用
   };

   ParticleMaterial();

   /// @brief 初期化
   /// @param device グラフィックスデバイス
   /// @param color 初期カラー
   void Create(GraphicsDevice* device, const Vector4& color = Vector4(1.0f, 1.0f, 1.0f, 1.0f));

   /// @brief テクスチャを設定
   void SetTexture(Texture* texture) { texture_ = texture; }

   /// @brief カラーを設定
   void SetColor(const Vector4& color);

   /// @brief UV変換行列を設定
   void SetUVTransform(const Matrix4x4& transform);

   /// @brief HDR 出力用の輝度倍率を設定する
   /// @param brightness 0以上の輝度倍率
   void SetBrightness(float brightness);

   /// @brief マスク描画に使用するアルファ破棄閾値を設定する
   /// @param cutoff 0から1の閾値
   void SetAlphaCutoff(float cutoff);

   /// @brief トゥーン階調数を設定する
   /// @param steps 2以上で有効、0または1で無効
   void SetToonSteps(uint32_t steps);

   /// @brief HDR 出力用の輝度倍率を取得する
   float GetBrightness() const;

   /// @brief アルファ破棄閾値を取得する
   float GetAlphaCutoff() const;

   /// @brief トゥーン階調数を取得する
   uint32_t GetToonSteps() const;

   /// @brief ソフトパーティクルを有効化する
   void SetSoftParticlesEnabled(bool enabled);

   /// @brief ソフトパーティクルが有効か取得する
   bool IsSoftParticlesEnabled() const { return softParticlesEnabled_; }

   /// @brief 交差境界をフェードする距離を設定する
   void SetSoftParticleDistance(float distance);

   /// @brief 交差境界をフェードする距離を取得する
   float GetSoftParticleDistance() const { return softParticleDistance_; }

   /// @brief 背景を歪ませる最大ピクセル数を設定する
   void SetDistortionStrength(float strength);

   /// @brief 背景を歪ませる最大ピクセル数を取得する
   float GetDistortionStrength() const { return distortionStrength_; }

   /// @brief 屈折結果と元背景の混合率を設定する
   void SetDistortionBlend(float blend);

   /// @brief 屈折結果と元背景の混合率を取得する
   float GetDistortionBlend() const { return distortionBlend_; }

   /// @brief 歪み方向にテクスチャのRGフローマップを使用するか設定する
   /// @param enabled trueならRGを方向、falseなら粒子中心からの放射方向を使用
   void SetDistortionUseTextureFlow(bool enabled);

   /// @brief 歪み方向にテクスチャのRGフローマップを使用するか取得する
   /// @return RGフローマップを使用する場合true
   bool IsDistortionUsingTextureFlow() const { return distortionUseTextureFlow_; }

   /// @brief 深度復元に必要な画面・カメラ情報を更新する
   void SetSceneParameters(float width, float height, float nearClip, float farClip, bool orthographic);

   /// @brief テクスチャを取得
   /// @return テクスチャへのポインタ
   Texture* GetTexture() const { return texture_; }

   /// @brief マテリアルデータを取得
   /// @return マテリアルデータへのポインタ
   MaterialData* GetMaterialData() const { return materialData_; }

   /// @brief マテリアルリソースを取得
   /// @return マテリアルリソースへのポインタ
   ID3D12Resource* GetMaterialResource() const override { return materialResource_.Get(); }

   /// @brief このマテリアルが使用するパイプライン名を取得（デフォルト: "Particle"）
   const std::string& GetPipelineName() const override { return pipelineName_; }

   /// @brief このマテリアルが使用するパイプライン名を設定
   void SetPipelineName(const std::string& name) { pipelineName_ = name; }

   /// @brief 優先するブレンドモードを設定（nullopt = デフォルト加算ブレンド）
   void SetBlendMode(std::optional<BlendMode> mode) { blendMode_ = mode; }

   /// @brief 優先するブレンドモードを取得
   std::optional<BlendMode> GetBlendMode() const { return blendMode_; }

private:
   Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_ = nullptr;
   MaterialData* materialData_ = nullptr;
   Texture* texture_ = nullptr;
   std::string pipelineName_ = "Particle";  ///< 使用するパイプライン名
   std::optional<BlendMode> blendMode_;     ///< 優先ブレンドモード（nullopt = 加算）
   float brightness_ = 1.0f;
   float alphaCutoff_ = 0.001f;
   uint32_t toonSteps_ = 0;
   bool softParticlesEnabled_ = false;
   float softParticleDistance_ = 0.5f;
   float distortionStrength_ = 0.0f;
   float distortionBlend_ = 1.0f;
   bool distortionUseTextureFlow_ = false;
};
}
