#include "pch.h"
#include "ResourceHelper.h"

namespace GameEngine {
ComPtr<ID3D12Resource> ResourceHelper::CreateBufferResource(ID3D12Device* device, size_t sizeInBytes) {
   // 頂点リソース用のヒープの設定
   D3D12_HEAP_PROPERTIES uploadHeapProperties{};
   uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
   // 頂点リソースの設定
   D3D12_RESOURCE_DESC bufferResourceDesc{};
   // バッファリソース。テクスチャの場合はまた別の設定をする
   bufferResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
   bufferResourceDesc.Width = sizeInBytes;// リソースのサイズ
   // バッファの場合はこれらは1にする決まり
   bufferResourceDesc.Height = 1;
   bufferResourceDesc.DepthOrArraySize = 1;
   bufferResourceDesc.MipLevels = 1;
   bufferResourceDesc.SampleDesc.Count = 1;
   // バッファの場合はこれにする決まり
   bufferResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
   // 実際に頂点リソースを作る
   ComPtr<ID3D12Resource> bufferResource;
   HRESULT hr = device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &bufferResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(bufferResource.GetAddressOf()));
   assert(SUCCEEDED(hr));

   return bufferResource;
}

ComPtr<ID3D12Resource> ResourceHelper::CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata) {
   D3D12_RESOURCE_DESC resourceDesc{};
   resourceDesc.Width = UINT(metadata.width);
   resourceDesc.Height = UINT(metadata.height);
   resourceDesc.MipLevels = UINT16(metadata.mipLevels);
   resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);
   resourceDesc.Format = metadata.format;
   resourceDesc.SampleDesc.Count = 1;
   resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);

   D3D12_HEAP_PROPERTIES heapProperties{};
   heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

   ComPtr<ID3D12Resource> resource = nullptr;
   HRESULT hr = device->CreateCommittedResource(
	  &heapProperties,
	  D3D12_HEAP_FLAG_NONE,
	  &resourceDesc,
	  D3D12_RESOURCE_STATE_COPY_DEST,
	  nullptr,
	  IID_PPV_ARGS(resource.GetAddressOf())
   );

   assert(SUCCEEDED(hr));

   return resource;
}

ComPtr<ID3D12Resource> ResourceHelper::CreateDepthStencilTextureResource(ID3D12Device* device, int32_t width, int32_t height) {
   // 生成するResourceの設定
   D3D12_RESOURCE_DESC resourceDesc{};
   resourceDesc.Width = width;
   resourceDesc.Height = height;
   resourceDesc.MipLevels = 1;
   resourceDesc.DepthOrArraySize = 1;
   resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
   resourceDesc.SampleDesc.Count = 1;
   resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
   resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

   // 利用するHeapの設定 
   D3D12_HEAP_PROPERTIES heapProperties{};
   heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

   // 深度値のクリア設定
   D3D12_CLEAR_VALUE depthClearValue{};
   depthClearValue.DepthStencil.Depth = 1.0f;
   depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

   ComPtr<ID3D12Resource> resource = nullptr;
   HRESULT hr = device->CreateCommittedResource(
	  &heapProperties,
	  D3D12_HEAP_FLAG_NONE,
	  &resourceDesc,
	  D3D12_RESOURCE_STATE_DEPTH_WRITE,
	  &depthClearValue,
	  IID_PPV_ARGS(resource.GetAddressOf())
   );

   assert(SUCCEEDED(hr));

   return resource;
}

ComPtr<ID3D12Resource> ResourceHelper::UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages, ID3D12Device* device, ID3D12GraphicsCommandList* commandList) {
   std::vector<D3D12_SUBRESOURCE_DATA> subresources;
   DirectX::PrepareUpload(device, mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);
   uint64_t intermediateSize = GetRequiredIntermediateSize(texture, 0, UINT(subresources.size()));
   ComPtr<ID3D12Resource> intermediateResource = CreateBufferResource(device, intermediateSize);
   UpdateSubresources(commandList, texture, intermediateResource.Get(), 0, 0, static_cast<UINT>(subresources.size()), subresources.data());
   // Textureへの転送後は利用できるよう、D3D12_RESOURCE_STATE_COPY_DESTからDESTからD3D12_RESOURCE_GENERIC_READへResourceStateを変更する
   CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
	  texture,
	  D3D12_RESOURCE_STATE_COPY_DEST,
	  D3D12_RESOURCE_STATE_GENERIC_READ,
	  D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES
   );

   commandList->ResourceBarrier(1, &barrier);

   return intermediateResource;
}
}