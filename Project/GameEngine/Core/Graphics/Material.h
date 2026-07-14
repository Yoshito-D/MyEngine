#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <string>
#include <optional>
#include "IMaterialData.h"
#include "Utility/VectorMath.h"
#include "Utility/MathUtils.h"
#include "PipelineState.h"

using namespace Microsoft::WRL;

namespace GameEngine {
class GraphicsDevice;

//// @brief マテリアルクラス
class Material : public IMaterialData {
public:
   /// @brief マテリアルデータ構造体
   struct MaterialData {
	  Vector4 color; // マテリアルの色
	  int32_t lightingMode; // ライティングを有効にするかどうか
	  float  environmentCoefficient; // 環境マップの強度
	  float  padding[2]; // パディング（uvTransformの16バイト境界アライメント用）
	  Matrix4x4 uvTransform; // UV座標に適用する変換行列
	  float shininess; // 光沢度
	  float rimLightIntensity; // リムライトの強度
	  float rimLightPower; // リムライトの減衰指数
	  float fillLightIntensity; // フィルライトの強度
	  Vector4 rimLightColor; // リムライトの色
	  Vector4 fillLightColor; // フィルライトの色
   };

   static_assert(sizeof(MaterialData) == 144, "MaterialData must match the HLSL constant-buffer layout");

   /// @brief ライティングモード列挙体
   enum LightingMode {
	  NONE = 0,
	  LAMBERT = 1,
	  HALFLAMBERT = 2,
	  PHONG = 3,
	  BLINNPHONG = 4
   };

   /// @brief デバイスを取得して初期化
   /// @param device デバイス
   static void Initialize(GraphicsDevice* device);

   /// @brief ID3D12Device を使用してオブジェクトを作成
   /// @param device Direct3D 12 デバイスへのポインタ
   /// @param color オブジェクトの色。デフォルトは 0xffffffff（白）
   /// @param enableLightingMode ライティングモード。デフォルトは LightingMode::HALFLAMBERT
   /// @param uvTransform UV座標に適用する 4x4 行列。デフォルトは単位行列
   void Create(unsigned int color = 0xffffffff, int32_t lightingMode = LightingMode::HALFLAMBERT, const Matrix4x4& uvTransform = MakeIdentity4x4(), float shininess = 40.0f);

   /// @brief マテリアルデータへのポインタを取得
   /// @return マテリアルデータへのポインタ
   MaterialData* GetMaterialData() const { return materialData_; }

   /// @brief マテリアルリソースを取得
   /// @return マテリアルリソースへのポインタ
   ID3D12Resource* GetMaterialResource() const override { return materialResource_.Get(); }

   // ========== プロパティアクセス関数 ==========

   /// @brief マテリアルの色を設定
   /// @param color 色（Vector4形式）
   void SetColor(const Vector4& color);

   /// @brief マテリアルの色を設定
   /// @param color 色（unsigned int形式）
   void SetColor(unsigned int color);

   /// @brief マテリアルの色を取得
   /// @return 色（Vector4形式）
   Vector4 GetColor() const;

   /// @brief ライティングモードを設定
   /// @param mode ライティングモード
   void SetLightingMode(LightingMode mode);

   /// @brief ライティングモードを取得
   /// @return ライティングモード
   LightingMode GetLightingMode() const;

   /// @brief このマテリアルが使用するパイプライン名を設定
   /// @param name パイプライン名（空文字列の場合はデフォルト "Object3D" にフォールバック）
   void SetPipelineName(const std::string& name) { pipelineName_ = name; }

   /// @brief このマテリアルが使用するパイプライン名を取得
   /// @return パイプライン名（空文字列 = デフォルト動作）
   const std::string& GetPipelineName() const override { return pipelineName_; }

   /// @brief マテリアルが優先するブレンドモードを設定
   /// @param mode ブレンドモード（std::nullopt で DrawCommand のモードにフォールバック）
   void SetBlendMode(std::optional<BlendMode> mode) { blendMode_ = mode; }

   /// @brief マテリアルが優先するブレンドモードを取得
   /// @return ブレンドモード（nullopt = DrawCommand のブレンドモードを使用）
   std::optional<BlendMode> GetBlendMode() const { return blendMode_; }

   /// @brief 光沢度を設定
   /// @param shininess 光沢度
   void SetShininess(float shininess);

   /// @brief 光沢度を取得
   /// @return 光沢度
   float GetShininess() const;

   /// @brief 環境テクスチャの強度を設定
   /// @param strength 強度 (0.0f〜1.0f)
   void SetEnvironmentTextureStrength(float strength);

   /// @brief 環境テクスチャの強度を取得
   /// @return 強度
   float GetEnvironmentTextureStrength() const;

   /// @brief リムライトの色を設定
   /// @param color リムライトの色
   void SetRimLightColor(const Vector4& color);

   /// @brief リムライトの色を取得
   /// @return リムライトの色
   Vector4 GetRimLightColor() const;

   /// @brief リムライトの強度を設定
   /// @param intensity 0以上の強度
   void SetRimLightIntensity(float intensity);

   /// @brief リムライトの強度を取得
   /// @return リムライトの強度
   float GetRimLightIntensity() const;

   /// @brief リムライトの減衰指数を設定
   /// @param power 0より大きい減衰指数。大きいほど輪郭が細くなる
   void SetRimLightPower(float power);

   /// @brief リムライトの減衰指数を取得
   /// @return リムライトの減衰指数
   float GetRimLightPower() const;

   /// @brief フィルライトの色を設定
   /// @param color フィルライトの色
   void SetFillLightColor(const Vector4& color);

   /// @brief フィルライトの色を取得
   /// @return フィルライトの色
   Vector4 GetFillLightColor() const;

   /// @brief フィルライトの強度を設定
   /// @param intensity 0以上の強度
   void SetFillLightIntensity(float intensity);

   /// @brief フィルライトの強度を取得
   /// @return フィルライトの強度
   float GetFillLightIntensity() const;

   // ========== UVTransform操作関数 ==========

   /// @brief UVTransformを設定
   /// @param transform 変換行列
   void SetUVTransform(const Matrix4x4& transform);

   /// @brief UVTransformを取得
   /// @return 変換行列
   Matrix4x4 GetUVTransform() const;

   /// @brief UVのスケール、回転、平行移動を個別に設定
   /// @param scale スケール（Vector2）
   /// @param rotation 回転（ラジアン）
   /// @param translation 平行移動（Vector2）
   void SetUVTransform(const Vector2& scale, float rotation, const Vector2& translation);

   /// @brief UVのスケールを設定
   /// @param scale スケール（Vector2）
   void SetUVScale(const Vector2& scale);

   /// @brief UVの回転を設定
   /// @param rotation 回転（ラジアン）
   void SetUVRotation(float rotation);

   /// @brief UVの平行移動を設定
   /// @param translation 平行移動（Vector2）
   void SetUVTranslation(const Vector2& translation);

   /// @brief UVのスケールを取得
   /// @return スケール（Vector2）
   Vector2 GetUVScale() const;

   /// @brief UVの回転を取得
   /// @return 回転（ラジアン）
   float GetUVRotation() const;

   /// @brief UVの平行移動を取得
   /// @return 平行移動（Vector2）
   Vector2 GetUVTranslation() const;

   /// @brief UVTransformをリセット（単位行列に戻す）
   void ResetUVTransform();

private:
   ComPtr<ID3D12Resource> materialResource_ = nullptr;
   MaterialData* materialData_ = nullptr;
   std::string pipelineName_;  ///< 使用するパイプライン名（空文字列 = デフォルト）
   std::optional<BlendMode> blendMode_;  ///< 優先ブレンドモード（nullopt = DrawCommand 依存）

   /// @brief 現在のUVTransformからスケール、回転、平行移動を分解
   /// @param outScale 出力：スケール
   /// @param outRotation 出力：回転
   /// @param outTranslation 出力：平行移動
   void DecomposeUVTransform(Vector2& outScale, float& outRotation, Vector2& outTranslation) const;

   /// @brief スケール、回転、平行移動からUVTransformを合成
   /// @param scale スケール
   /// @param rotation 回転
   /// @param translation 平行移動
   void ComposeUVTransform(const Vector2& scale, float rotation, const Vector2& translation);
};
}
