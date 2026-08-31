#pragma once
#include "PostProcess.h"
#include "Utility/Math/Vector2.h"
#include <d3d12.h>
#include <string>
#include <wrl.h>

namespace GameEngine {
class Texture;

/// @brief ディゾルブの見た目とマスクUVをまとめた実行時パラメーター
struct DissolveParams {
   float threshold = 0.0f;                                  ///< 可視領域と消失領域を分けるしきい値
   float edgeWidth = 0.08f;                                 ///< しきい値周辺の遷移幅
   float edgeIntensity = 1.0f;                              ///< 遷移エッジの発光強度
   float maskContrast = 1.0f;                               ///< マスク値を0.5中心で強調する倍率
   Vector2 maskTiling = { 1.0f, 1.0f };                     ///< マスクUVの繰り返し倍率
   Vector2 maskOffset = { 0.0f, 0.0f };                     ///< マスクUVのスクロール量
   float edgeColor[4] = { 1.0f, 0.55f, 0.08f, 1.0f };      ///< 遷移エッジのRGBA色
   float dissolveColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };    ///< 消失側へ補間するRGBA色
};

/// @brief マスクテクスチャを使ったディゾルブ効果
class Dissolve : public PostProcess {
public:
   /// @brief HLSLのDissolveCBと同じ定数バッファレイアウト
   struct DissolveCB {
	  float threshold;          ///< 可視／消失境界のしきい値
	  float edgeWidth;          ///< 境界の遷移幅
	  float edgeIntensity;      ///< エッジ色の強度
	  float maskContrast;       ///< マスク値のコントラスト倍率
	  Vector2 maskTiling;       ///< マスクUVの繰り返し倍率
	  Vector2 maskOffset;       ///< マスクUVのオフセット
	  float edgeColor[4];       ///< 境界へ合成するRGBA色
	  float dissolveColor[4];   ///< 消失側へ補間するRGBA色
   };

   /// @brief 共有描画環境とディゾルブ用定数バッファを初期化する。
   /// @param device グラフィックスデバイス
   /// @param renderTarget ポストプロセスの出力先
   void Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) override;

   /// @brief 入力カラーへ2Dマスクを使ったディゾルブを適用する。
   /// @param inputSRV 入力カラーのSRV
   void Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) override;

#ifdef USE_IMGUI
   /// @brief ディゾルブ設定とマスクプレビューのエディターUIを描画する。
   void ImGuiEdit() override;
#endif

   /// @brief エディター表示と保存識別に使用するエフェクト名を取得する。
   /// @return エフェクト名
   const char* GetEffectName() const override { return "Dissolve"; }

   /// @copydoc PostProcess::SerializeSettings
   nlohmann::json SerializeSettings() const override;
   /// @copydoc PostProcess::DeserializeSettings
   bool DeserializeSettings(const nlohmann::json& settings) override;

   /// @brief 全数値パラメーターを有効範囲へ正規化して設定する。
   /// @param params 反映するディゾルブパラメーター
   void SetParams(const DissolveParams& params);

   /// @brief 現在の正規化済みパラメーターを取得する。
   /// @return 現在のディゾルブパラメーター
   const DissolveParams& GetParams() const { return params_; }

   /// @brief ディゾルブの進行しきい値を設定する。
   /// @param threshold 0～1へ正規化されるしきい値
   void SetThreshold(float threshold);

   /// @brief しきい値周辺の遷移エッジ幅を設定する。
   /// @param edgeWidth 0.0001～0.5へ正規化される幅
   void SetEdgeWidth(float edgeWidth);

   /// @brief マスクに使用する2Dテクスチャの登録名を設定する。
   /// @param textureName EngineContextへ登録されたテクスチャ名
   void SetMaskTextureName(const std::string& textureName);

   /// @brief 現在指定されているマスクテクスチャ名を取得する。
   /// @return マスクテクスチャの登録名
   const std::string& GetMaskTextureName() const { return maskTextureName_; }

private:
   DissolveParams params_;
   std::string maskTextureName_ = "engine/textures/postprocess/noise0.png";
   Texture* maskTexture_ = nullptr;
   bool maskTextureLookupDirty_ = true;

   Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
   DissolveCB* constantBufferData_ = nullptr;

   void CreateConstantBuffer();
   void UpdateConstantBuffer();
   Texture* ResolveMaskTexture();
};
}
