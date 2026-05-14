#include "pch.h"
#include "LightDataBuffer.h"
#include "GraphicsDevice.h"
#include "ResourceHelper.h"

namespace GameEngine {
namespace {
GraphicsDevice* sDevice_ = nullptr;
bool sIsInitialized_ = false;
}

void LightDataBuffer::Initialize(GraphicsDevice* device) {
	if (sIsInitialized_) return;
	sDevice_ = device;
	sIsInitialized_ = true;
}

void LightDataBuffer::Create(uint32_t maxDirectionalLights, uint32_t maxPointLights, uint32_t maxSpotLights, uint32_t maxAreaLights) {
	if (!sIsInitialized_) return;

	maxDirectionalLights_ = maxDirectionalLights;
	maxPointLights_ = maxPointLights;
	maxSpotLights_ = maxSpotLights;
	maxAreaLights_ = maxAreaLights;

	// 各ライトタイプの構造化バッファを作成
	CreateStructuredBuffer(directionalLightBuffer_, directionalLightSRVHandle_, sizeof(DirectionalLightData), maxDirectionalLights_);
	CreateStructuredBuffer(pointLightBuffer_, pointLightSRVHandle_, sizeof(PointLightData), maxPointLights_);
	CreateStructuredBuffer(spotLightBuffer_, spotLightSRVHandle_, sizeof(SpotLightData), maxSpotLights_);
	CreateStructuredBuffer(areaLightBuffer_, areaLightSRVHandle_, sizeof(AreaLightData), maxAreaLights_);

	// ライトカウント用の定数バッファを作成
	lightCountResource_ = ResourceHelper::CreateBufferResource(sDevice_->GetDevice(), sizeof(LightCountData));
	lightCountResource_->Map(0, nullptr, reinterpret_cast<void**>(&lightCountData_));

	// マップ処理
	directionalLightBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData_));
	pointLightBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&pointLightData_));
	spotLightBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&spotLightData_));
	areaLightBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&areaLightData_));

	// 初期化
	UpdateLightCount();
}

void LightDataBuffer::UpdateDirectionalLights(const std::vector<DirectionalLightData>& lights) {
	if (!directionalLightData_) return;

	currentDirectionalLightCount_ = static_cast<uint32_t>(std::min(lights.size(), static_cast<size_t>(maxDirectionalLights_)));
	for (uint32_t i = 0; i < currentDirectionalLightCount_; ++i) {
		directionalLightData_[i] = lights[i];
	}

	UpdateLightCount();
}

void LightDataBuffer::UpdatePointLights(const std::vector<PointLightData>& lights) {
	if (!pointLightData_) return;

	currentPointLightCount_ = static_cast<uint32_t>(std::min(lights.size(), static_cast<size_t>(maxPointLights_)));
	for (uint32_t i = 0; i < currentPointLightCount_; ++i) {
		pointLightData_[i] = lights[i];
	}

	UpdateLightCount();
}

void LightDataBuffer::UpdateSpotLights(const std::vector<SpotLightData>& lights) {
	if (!spotLightData_) return;

	currentSpotLightCount_ = static_cast<uint32_t>(std::min(lights.size(), static_cast<size_t>(maxSpotLights_)));
	for (uint32_t i = 0; i < currentSpotLightCount_; ++i) {
		spotLightData_[i] = lights[i];
	}

	UpdateLightCount();
}

void LightDataBuffer::UpdateAreaLights(const std::vector<AreaLightData>& lights) {
	if (!areaLightData_) return;

	currentAreaLightCount_ = static_cast<uint32_t>(std::min(lights.size(), static_cast<size_t>(maxAreaLights_)));
	for (uint32_t i = 0; i < currentAreaLightCount_; ++i) {
		areaLightData_[i] = lights[i];
	}

	UpdateLightCount();
}

void LightDataBuffer::UpdateLightCount() {
	if (!lightCountData_) return;

	lightCountData_->directionalLightCount = currentDirectionalLightCount_;
	lightCountData_->pointLightCount = currentPointLightCount_;
	lightCountData_->spotLightCount = currentSpotLightCount_;
	lightCountData_->areaLightCount = currentAreaLightCount_;
}

void LightDataBuffer::CreateStructuredBuffer(ComPtr<ID3D12Resource>& buffer, D3D12_GPU_DESCRIPTOR_HANDLE& srvHandle, uint32_t elementSize, uint32_t elementCount) {
	// バッファサイズを計算（最低1要素は確保）
	uint32_t bufferSize = elementSize * std::max(elementCount, 1u);

	// 構造化バッファ用のヒーププロパティ
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	// リソースデスクリプタ
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = bufferSize;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	// バッファ作成
	HRESULT hr = sDevice_->GetDevice()->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(buffer.GetAddressOf())
	);
	assert(SUCCEEDED(hr));

	// SRVを作成
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = elementCount;
	srvDesc.Buffer.StructureByteStride = elementSize;
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	// ディスクリプタヒープにSRVを作成
	UINT srvIndex = sDevice_->GetNextSrvIndex();
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = sDevice_->GetSRVHeap()->GetCPUDescriptorHandleForHeapStart();
	cpuHandle.ptr += srvIndex * sDevice_->GetDescriptorSizeCBVSRVUAV();
	
	sDevice_->GetDevice()->CreateShaderResourceView(buffer.Get(), &srvDesc, cpuHandle);
	
	// インデックスをインクリメント
	sDevice_->IncrementSrvIndex();

	// GPUハンドルを取得
	srvHandle = sDevice_->GetSRVHeap()->GetGPUDescriptorHandleForHeapStart();
	srvHandle.ptr += srvIndex * sDevice_->GetDescriptorSizeCBVSRVUAV();
}

}
