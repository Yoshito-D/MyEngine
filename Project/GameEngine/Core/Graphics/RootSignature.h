#pragma once
#include <d3d12.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <vector>

using namespace Microsoft::WRL;

namespace GameEngine {
/// @brief ルートシグネチャクラス
class RootSignature {
public:
   /// @brief ルートシグネチャを生成する
   /// @param device デバイス
   void CreateRootSignature(ID3D12Device* device);

   /// @brief ルートパラメータを設定する
   /// @param type ルートパラメータのタイプ
   /// @param visibility シェーダーの可視性
   /// @param shaderRegister シェーダーレジスタ番号
   /// @param descriptorRanges ディスクリプタレンジの配列
   /// @param numDescriptorRanges ディスクリプタレンジの数
   void SetRootParameter(
	  D3D12_ROOT_PARAMETER_TYPE type,
	  D3D12_SHADER_VISIBILITY visibility,
	  UINT shaderRegister = 0,
	  const D3D12_DESCRIPTOR_RANGE* descriptorRanges = nullptr,
	  UINT numDescriptorRanges = 0,
	  UINT num32BitValues = 0
   );

   /// @brief スタティックサンプラーを設定する
   /// @param filter フィルタ
   ///　@param addressU Uアドレスモード
   ///　@param addressV Vアドレスモード
   ///　@param addressW Wアドレスモード
   ///　@param comparisonFunc 比較関数
   ///　@param maxLOD 最大LOD
   ///　@param shaderRegister シェーダーレジスタ番号
   ///　@param shaderVisibility シェーダーの可視性
   void SetSampler(
	  D3D12_FILTER filter,
	  D3D12_TEXTURE_ADDRESS_MODE addressU,
	  D3D12_TEXTURE_ADDRESS_MODE addressV,
	  D3D12_TEXTURE_ADDRESS_MODE addressW,
	  D3D12_COMPARISON_FUNC comparisonFunc,
	  FLOAT maxLOD,
	  UINT shaderRegister,
	  D3D12_SHADER_VISIBILITY shaderVisibility
   );

   /// @brief ディスクリプタレンジを作成する
   /// @param rangeType レンジのタイプ（SRV、UAV、CBV）
   /// @param numDescriptors ディスクリプタ数
   /// @param baseShaderRegister ベースシェーダーレジスタ
   /// @param registerSpace レジスタスペース
   /// @return 作成されたディスクリプタレンジへのポインタ
   const D3D12_DESCRIPTOR_RANGE* CreateDescriptorRange(
	  D3D12_DESCRIPTOR_RANGE_TYPE rangeType,
	  UINT numDescriptors,
	  UINT baseShaderRegister,
	  UINT registerSpace = 0
   );

   /// @brief ルートシグネチャを取得する
   /// @return ルートシグネチャのポインタ
   ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }

   /// @brief ルートシグネチャフラグを設定する
   /// @param flags ルートシグネチャフラグ
   void SetFlags(D3D12_ROOT_SIGNATURE_FLAGS flags);

private:
   ComPtr<ID3DBlob> rootSignatureBlob_ = nullptr;
   ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
   std::vector<D3D12_ROOT_PARAMETER> rootParameters_;
   std::vector<D3D12_STATIC_SAMPLER_DESC> staticSamplers_;
   D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags_ = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
   
   // ディスクリプタレンジの動的配列
   std::vector<D3D12_DESCRIPTOR_RANGE> descriptorRanges_;
};
}
