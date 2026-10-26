#include "GameEngine/pch.h"
#include "GameEngine/Graphics/Resources/Texture.h"
#include "GameEngine/Graphics/Device/ResourceHelper.h"
#include "GameEngine/Graphics/Device/GraphicsDevice.h"
#include <filesystem>

namespace GameEngine {
ComPtr<ID3D12Resource> Texture::LoadTexture(
   GraphicsDevice* device, const std::filesystem::path& filePath, const TextureInfo& info) {
   if (!device || !device->GetDevice() || !device->GetCommandList()) {
      return nullptr;
   }

   name_ = info.name;
   DirectX::ScratchImage mipImages;
   // 実行時にはビルド済みDDSだけを展開し、画像変換とミップ生成は行わない。
   if (FAILED(DirectX::LoadFromDDSFile(filePath.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, mipImages))) {
      return nullptr;
   }

   if (!mipImages.GetImageCount()) {
      return nullptr;
   }

   metadata_ = mipImages.GetMetadata();
   // 壊れた生成物や異なる世代のカタログをGPUリソース生成前に拒否する。
   if (metadata_.width != info.width || metadata_.height != info.height || metadata_.mipLevels != info.mipLevels ||
       metadata_.format != info.format || metadata_.IsCubemap() != info.isCubemap) {
      return nullptr;
   }

   if (metadata_.dimension != DirectX::TEX_DIMENSION_TEXTURE2D || !metadata_.width || !metadata_.height ||
       metadata_.width > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
       metadata_.height > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION || !metadata_.mipLevels ||
       metadata_.mipLevels > D3D12_REQ_MIP_LEVELS ||
       (metadata_.IsCubemap() ? metadata_.arraySize != 6 : metadata_.arraySize != 1)) {
      Logger::Warning("Unsupported texture dimensions: " + info.name);
      return nullptr;
   }

   D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport{metadata_.format};

   if (FAILED(device->GetDevice()->CheckFeatureSupport(
          D3D12_FEATURE_FORMAT_SUPPORT, &formatSupport, sizeof(formatSupport))) ||
       !(formatSupport.Support1 & D3D12_FORMAT_SUPPORT1_TEXTURE2D) ||
       !(formatSupport.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE)) {
      Logger::Warning("Unsupported texture format: " + info.name);
      return nullptr;
   }

   // テクスチャのサイズ情報を保存
   width_ = static_cast<uint32_t>(metadata_.width);
   height_ = static_cast<uint32_t>(metadata_.height);

   // DEFAULTヒープの実体へコピーを記録し、中間UPLOADリソースを呼び出し元へ返す。
   // コマンド完了前に中間リソースが破棄されないよう、寿命管理を外側へ委ねている。
   textureResource_ = ResourceHelper::CreateTextureResource(device->GetDevice(), metadata_);
   ComPtr<ID3D12Resource> intermediateResource = ResourceHelper::UploadTextureData(
      textureResource_.Get(), mipImages, device->GetDevice(), device->GetCommandList());

   D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
   srvDesc.Format = metadata_.format;
   srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

   if (metadata_.IsCubemap()) {
      // DDSの6面配列はTexture2DArrayではなくTextureCubeとして公開し、
      // シェーダー側が方向ベクトルでサンプリングできるようにする。
      srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
      srvDesc.TextureCube.MostDetailedMip = 0;
      srvDesc.TextureCube.MipLevels = UINT_MAX;
      srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
   } else {
      srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
      srvDesc.Texture2D.MipLevels = static_cast<UINT>(metadata_.mipLevels);
   }

   // CPU/GPUハンドルは同じディスクリプタ番号を指す。CPU側でビューを書き込み、
   // 描画時には対応するGPU側ハンドルだけをルートテーブルへ渡す。
   UINT index = device->GetNextSrvIndex();

   textureSrvHandleCPU_ = CD3DX12_CPU_DESCRIPTOR_HANDLE(
      device->GetSRVHeap()->GetCPUDescriptorHandleForHeapStart(), index, device->GetDescriptorSizeCBVSRVUAV());

   textureSrvHandleGPU_ = CD3DX12_GPU_DESCRIPTOR_HANDLE(
      device->GetSRVHeap()->GetGPUDescriptorHandleForHeapStart(), index, device->GetDescriptorSizeCBVSRVUAV());

   // SRVの生成
   device->GetDevice()->CreateShaderResourceView(textureResource_.Get(), &srvDesc, textureSrvHandleCPU_);

   device->IncrementSrvIndex();

   return intermediateResource;
}
}
