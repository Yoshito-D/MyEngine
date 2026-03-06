#include "pch.h"
#include "Texture.h"
#include "ResourceHelper.h"
#include "GraphicsDevice.h"

namespace {
Logger& log_ = Logger::GetInstance();
}

namespace GameEngine {
ComPtr<ID3D12Resource> Texture::LoadTexture(GraphicsDevice* device, const std::string& filePath) {
   name_ = filePath.substr(filePath.find_last_of("/\\") + 1);
   DirectX::ScratchImage mipImages = LoadTextureWithMipmaps(filePath);
   metadata_ = mipImages.GetMetadata();

   // テクスチャのサイズ情報を保存
   width_ = static_cast<uint32_t>(metadata_.width);
   height_ = static_cast<uint32_t>(metadata_.height);

   textureResource_ = ResourceHelper::CreateTextureResource(device->GetDevice(), metadata_);
   ComPtr<ID3D12Resource> intermediateResource = ResourceHelper::UploadTextureData(textureResource_.Get(), mipImages, device->GetDevice(), device->GetCommandList());

   D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
   srvDesc.Format = metadata_.format;
   srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
   srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
   srvDesc.Texture2D.MipLevels = static_cast<UINT>(metadata_.mipLevels);

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

DirectX::ScratchImage Texture::LoadTextureWithMipmaps(const std::string& filePath) {
   DirectX::ScratchImage image{};
   std::wstring filePathW = log_.ConvertString(filePath);

   HRESULT hr = DirectX::LoadFromWICFile(
	  filePathW.c_str(),
	  DirectX::WIC_FLAGS_FORCE_SRGB,
	  nullptr,
	  image
   );

   assert(SUCCEEDED(hr));

   const DirectX::TexMetadata& metadata = image.GetMetadata();

   // ミップマップが生成できるサイズかチェック
   if (metadata.width <= 1 && metadata.height <= 1) {
	  // 1x1など、ミップマップ不要（または生成できない）場合はそのまま返す
	  return image;
   }

   DirectX::ScratchImage mipImages{};
   hr = DirectX::GenerateMipMaps(
	  image.GetImages(),
	  image.GetImageCount(),
	  metadata,
	  DirectX::TEX_FILTER_SRGB,
	  0,
	  mipImages
   );

   if (FAILED(hr)) {
	  log_.Log("MipMap generation failed for: " + filePath, Logger::LogLevel::Error);
	  return image; // ミップマップ生成に失敗した場合、元画像を返す
   }

   return mipImages;
}
}