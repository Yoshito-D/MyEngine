#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <string>
#include "../../../externals/DirectXTex/DirectXTex.h"
#include "../../../externals/DirectXTex/d3dx12.h"

using namespace Microsoft::WRL;

namespace GameEngine {
class GraphicsDevice;

/// @brief テクスチャクラス
class Texture {
public:
   /// @brief テクスチャをロードする
   /// @param device グラフィックスデバイス
   /// @param filePath テクスチャファイルのパス
   /// @return ロードされたテクスチャリソース
   [[nodiscard]] ComPtr<ID3D12Resource> LoadTexture(GraphicsDevice* device, const std::string& filePath);

   /// @brief テクスチャのリソースを取得する
   /// @return テクスチャリソース
   const CD3DX12_GPU_DESCRIPTOR_HANDLE& GetTextureSrvHandleGPU() { return textureSrvHandleGPU_; }

   /// @brief テクスチャのSRVハンドルをCPU側で取得する
   /// @return テクスチャのSRVハンドル（CPU側）
   const CD3DX12_CPU_DESCRIPTOR_HANDLE& GetTextureSrvHandleCPU() { return textureSrvHandleCPU_; }

   /// @brief テクスチャのリソースを取得する
   /// @return テクスチャの名前
   const std::string& GetName() const { return name_; }

   /// @brief テクスチャの幅を取得する
   /// @return テクスチャの幅（ピクセル）
   uint32_t GetWidth() const { return width_; }

   /// @brief テクスチャの高さを取得する
   /// @return テクスチャの高さ（ピクセル）
   uint32_t GetHeight() const { return height_; }

   /// @brief テクスチャのサイズを取得する
   /// @return テクスチャのサイズ（幅、高さ）
   std::pair<uint32_t, uint32_t> GetSize() const { return { width_, height_ }; }

   /// @brief テクスチャのメタデータを取得する
   /// @return テクスチャのメタデータ
   const DirectX::TexMetadata& GetMetadata() const { return metadata_; }

private:
   ComPtr<ID3D12Resource> textureResource_ = nullptr;
   CD3DX12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU_;
   CD3DX12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU_;
   std::string name_;
   uint32_t width_ = 0;   // テクスチャの幅
   uint32_t height_ = 0;  // テクスチャの高さ

   DirectX::TexMetadata metadata_;
private:
   /// @brief ミップマップを含むテクスチャをロードする
   /// @param filePath テクスチャファイルのパス
   /// @return ロードされたテクスチャのScratchImage
   DirectX::ScratchImage LoadTextureWithMipmaps(const std::string& filePath);
};
}