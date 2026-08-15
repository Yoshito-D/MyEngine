#include "pch.h"
#include "Texture.h"
#include "ResourceHelper.h"
#include "GraphicsDevice.h"

namespace GameEngine {
ComPtr<ID3D12Resource> Texture::LoadTexture(GraphicsDevice* device, const std::string& filePath) {
   name_ = filePath.substr(filePath.find_last_of("/\\") + 1);
   DirectX::ScratchImage mipImages = LoadTextureWithMipmaps(filePath);
   metadata_ = mipImages.GetMetadata();

   // テクスチャのサイズ情報を保存
   width_ = static_cast<uint32_t>(metadata_.width);
   height_ = static_cast<uint32_t>(metadata_.height);

   // DEFAULTヒープの実体へコピーを記録し、中間UPLOADリソースを呼び出し元へ返す。
   // コマンド完了前に中間リソースが破棄されないよう、寿命管理を外側へ委ねている。
   textureResource_ = ResourceHelper::CreateTextureResource(device->GetDevice(), metadata_);
   ComPtr<ID3D12Resource> intermediateResource = ResourceHelper::UploadTextureData(textureResource_.Get(), mipImages, device->GetDevice(), device->GetCommandList());

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

DirectX::ScratchImage Texture::LoadTextureWithMipmaps(const std::string& filePath) {
   DirectX::ScratchImage image{};
   std::wstring filePathW = Logger::ConvertString(filePath);

   HRESULT hr;

   if (filePathW.ends_with(L".dds")) {
      // DDSは既存の圧縮形式やキューブ面を保持し、一般画像は色空間をsRGBとして読み込む。
	  hr = DirectX::LoadFromDDSFile(filePathW.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
   } else {
	  hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
   }

   assert(SUCCEEDED(hr));

   const DirectX::TexMetadata& metadata = image.GetMetadata();

   // ミップマップが生成できるサイズかチェック
   if (metadata.width <= 1 && metadata.height <= 1) {
	  // 1x1など、ミップマップ不要（または生成できない）場合はそのまま返す
	  return image;
   }

   DirectX::ScratchImage mipImages{};

   if (DirectX::IsCompressed(image.GetMetadata().format)) {
      // BC圧縮画像をCPUで再生成すると形式と既存mipを失うため、そのままGPUへ送る。
	  mipImages = std::move(image); // 圧縮テクスチャは元画像をそのまま使用
   } else {
	  hr = DirectX::GenerateMipMaps(
		 image.GetImages(),
		 image.GetImageCount(),
		 metadata,
		 DirectX::TEX_FILTER_SRGB,
		 4,
		 mipImages
	  );
   }

   if (FAILED(hr)) {
	  Logger::Error("MipMap generation failed for: " + filePath);
	  return image; // ミップマップ生成に失敗した場合、元画像を返す
   }

   return mipImages;
}
}
