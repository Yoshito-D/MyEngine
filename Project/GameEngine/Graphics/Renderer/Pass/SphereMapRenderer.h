#pragma once

#include "GameEngine/UI/Text/TextTypes.h"
#include <cstdint>
#include <memory>
#include <vector>
#include <wrl.h>

namespace GameEngine {

class GraphicsDevice;
class PSOManager;
class SrvDescriptorAllocation;

/// @brief 透視投影する球の描画情報。
struct SphereMapSphere {
   Vector3 center;  ///< カメラ空間における中心（m）。Xは右、Yは下、Zは前方。
   float radius;    ///< 球の半径（m）。
   float emphasis;  ///< 緯線と経線の発光の強調度（0〜1）。
   Vector4 color;   ///< 線形RGBA色。
   Vector3 axisX;   ///< カメラ空間での惑星ローカルX方向。経線の基準となる単位ベクトル。
   Vector3 axisY;   ///< カメラ空間での惑星ローカルY方向。極を指す単位ベクトル。
};

/// @brief 球の発光に重ねる発光点の描画情報。
struct SphereMapPoint {
   Vector2 center;  ///< 描画領域の左上からの位置（UIピクセル）。
   float radius;    ///< 発光点の描画半径（UIピクセル）。
   float intensity; ///< 発光の強さ（0〜1）。
   Vector4 color;   ///< 線形RGBA色。
};

/// @brief スクリーンUIへ合成する球体マップのフレーム情報。
struct SphereMapFrame {
   std::uint32_t width;       ///< パネル幅（基準UIピクセル）。
   std::uint32_t height;      ///< パネル高（基準UIピクセル）。
   float displayScale = 1.f; ///< 描画結果を画面へ合成するときの縦横共通倍率。
   float margin;             ///< 基準UIの右端・下端からの余白（ピクセル）。
   Vector2 projectionCenter; ///< 光軸が描画領域と交わる位置（UIピクセル）。
   float apertureRadius;     ///< 光軸を中心とした円形表示の半径（UIピクセル）。
   Vector4 apertureColor;    ///< 円周の発光色と強度（線形RGBA）。
   Vector4 backgroundColor;  ///< 円内の背景色と不透明度（線形RGBA）。
   float focalLength;        ///< 透視投影の焦点距離（UIピクセル）。
   float nearDepth;          ///< カメラから手前側のクリップ面までの距離（m）。
   std::vector<SphereMapSphere> spheres; ///< 球の一覧。
   std::vector<SphereMapPoint> points;   ///< 球の発光に重ねる光点。
   std::string caption;      ///< マップ下部に中央揃えで表示する文字列。
   TextStyle captionStyle;   ///< 文字設定。位置とピボットは描画領域から求める。

   /// @brief 画面右下から距離文字列の下端中央までの差分（UIピクセル、Yは下向き）を返す。
   Vector2 GetCaptionOffset() const {
      return {-margin - static_cast<float>(width) * displayScale * 0.5f, -margin};
   }
};

/// @brief 専用の色ターゲットに球体マップの発光を加算する。
class SphereMapRenderer final {
public:
   /// @brief フレームの提出内容を破棄する。GPU資源は次フレームで再利用する。
   void BeginFrame();

   /// @brief 今フレームに描くマップを値で保持する。
   /// @param frame カメラ空間の球、投影設定および配置済みの発光点。
   void Submit(const SphereMapFrame& frame);

   /// @brief 今フレームの描画情報を取得する。未提出ならnullptr。
   const SphereMapFrame* GetFrame() const;

   /// @brief 専用ターゲットへ描き、合成用のSRVを返す。呼び出し側は描画先とPSOを再設定する。
   /// @param device コマンドを記録するデバイス。
   /// @param pipelines 球体マップの描画パイプライン。
   /// @param colorFormat 共通の中間描画先と同じ色情報の形式。
   D3D12_GPU_DESCRIPTOR_HANDLE Render(GraphicsDevice& device, const PSOManager& pipelines, DXGI_FORMAT colorFormat);

private:
   void PrepareTarget(GraphicsDevice& device, DXGI_FORMAT colorFormat);

   SphereMapFrame frame_{};
   bool submitted_ = false;
   std::uint32_t width_ = 0;
   std::uint32_t height_ = 0;
   Microsoft::WRL::ComPtr<ID3D12Resource> color_;
   Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
   std::shared_ptr<SrvDescriptorAllocation> srv_;
   D3D12_GPU_DESCRIPTOR_HANDLE srvHandle_{};
};

} // namespace GameEngine
