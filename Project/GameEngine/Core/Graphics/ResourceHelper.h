#pragma once
#include <d3d12.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <cstdint>
#include "../../externals/DirectXTex/DirectXTex.h"

using namespace Microsoft::WRL;

namespace GameEngine {
namespace ResourceHelper {
/// @brief バッファリソースを作成する
/// @param device デバイス
/// @param sizeInBytes バッファのサイズ（バイト単位）
/// @return バッファリソース
ComPtr<ID3D12Resource> CreateBufferResource(ID3D12Device* device, size_t sizeInBytes);

/// @brief テクスチャリソースを作成する
/// @param device デバイス
/// @param metadata テクスチャのメタデータ
/// @return テクスチャリソース
ComPtr<ID3D12Resource> CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata);

/// @brief 深度ステンシルテクスチャリソースを作成する
/// @param device デバイス
/// @param width 幅
/// @param height 高さ
/// @return 深度ステンシルテクスチャリソース
ComPtr<ID3D12Resource> CreateDepthStencilTextureResource(ID3D12Device* device, int32_t width, int32_t height);

/// @brief テクスチャのデータをアップロードする
/// @param texture テクスチャリソース
/// @param mipImages ミップマップ画像
/// @param device デバイス
/// @param commandList コマンドリスト
/// @return アップロードされたテクスチャリソース
[[nodiscard]] ComPtr<ID3D12Resource> UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages, ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
}
}