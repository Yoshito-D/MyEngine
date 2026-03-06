#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

#include "Utility/VectorMath.h"
#include "Utility/MathUtils.h"

using namespace Microsoft::WRL;

namespace GameEngine {
class GraphicsDevice;
/// @brief トランスフォーメーションマトリックスクラス
class TransformationMatrix {
public:
   /// @brief トランスフォーメーションマトリックスのデータ構造
   struct TransformationMatrixData {
	  Matrix4x4 wVP;// ワールド・ビュー・プロジェクション行列
	  Matrix4x4 world;// ワールド行列
	  Matrix4x4 worldInverseTranspose;// ワールド行列の逆行列の転置行列
   };

   /// @brief デバイスを取得して初期化
   /// @param device デバイス
   static void Initialize(GraphicsDevice* device);

   /// @brief トランスフォーメーションマトリックスを作成する
   /// @param device デバイス
   /// @param wvp ワールド・ビュー・プロジェクション行列
   /// @param world ワールド行列
   void Create(const Matrix4x4& wvp = MakeIdentity4x4(), const Matrix4x4& world = MakeIdentity4x4());

   /// @brief トランスフォーメーションマトリックスのデータを取得する
   /// @return トランスフォーメーションマトリックスデータのポインタ
   TransformationMatrixData* GetTransformationMatrixData() const { return transformationMatrixData_; }

   /// @brief トランスフォーメーションマトリックスのリソースを取得する
   /// @return トランスフォーメーションマトリックスリソースのポインタ
   ID3D12Resource* GetTransformationMatrixResource() const { return transformationMatrixResource_.Get(); }

private:
   ComPtr<ID3D12Resource> transformationMatrixResource_ = nullptr;
   TransformationMatrixData* transformationMatrixData_ = nullptr;
};
}