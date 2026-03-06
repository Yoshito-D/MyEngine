#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include "Utility/VectorMath.h"

using namespace Microsoft::WRL;

namespace GameEngine {
class GraphicsDevice;

/// @brief ライト用データバッファクラス（構造化バッファ管理）
class LightDataBuffer {
public:
	/// @brief ディレクショナルライトデータ構造体
	struct DirectionalLightData {
		Vector4 color;
		Vector3 direction;
		float intensity;
	};

	/// @brief ポイントライトデータ構造体
	struct PointLightData {
		Vector4 color;
		Vector3 position;
		float intensity;
		float radius;
		float decay;
		float padding[2];
	};

	/// @brief スポットライトデータ構造体
	struct SpotLightData {
		Vector4 color;
		Vector3 position;
		float intensity;
		Vector3 direction;
		float distance;
		float decay;
		float cosAngle;
		float cosFalloffStart;
		float padding;
	};

	/// @brief エリアライトデータ構造体
	struct AreaLightData {
		Vector4 color;
		Vector3 position;
		float intensity;
		Vector3 normal;
		float width;
		Vector3 tangent;
		float height;
		Vector3 padding;
		float padding2;
	};

	/// @brief ライトカウント構造体
	struct LightCountData {
		uint32_t directionalLightCount;
		uint32_t pointLightCount;
		uint32_t spotLightCount;
		uint32_t areaLightCount;
	};

	/// @brief デバイスを取得して初期化
	/// @param device デバイス
	static void Initialize(GraphicsDevice* device);

	/// @brief 構造化バッファを作成
	/// @param maxDirectionalLights ディレクショナルライトの最大数
	/// @param maxPointLights ポイントライトの最大数
	/// @param maxSpotLights スポットライトの最大数
	/// @param maxAreaLights エリアライトの最大数
	void Create(uint32_t maxDirectionalLights = 1, uint32_t maxPointLights = 32, uint32_t maxSpotLights = 32, uint32_t maxAreaLights = 16);

	/// @brief ディレクショナルライトデータを更新
	/// @param lights ライトデータの配列
	void UpdateDirectionalLights(const std::vector<DirectionalLightData>& lights);

	/// @brief ポイントライトデータを更新
	/// @param lights ライトデータの配列
	void UpdatePointLights(const std::vector<PointLightData>& lights);

	/// @brief スポットライトデータを更新
	/// @param lights ライトデータの配列
	void UpdateSpotLights(const std::vector<SpotLightData>& lights);

	/// @brief エリアライトデータを更新
	/// @param lights ライトデータの配列
	void UpdateAreaLights(const std::vector<AreaLightData>& lights);

	/// @brief ライトカウントを更新
	void UpdateLightCount();

	/// @brief ディレクショナルライトSRVハンドルを取得
	/// @return SRVのGPUハンドル
	D3D12_GPU_DESCRIPTOR_HANDLE GetDirectionalLightSRV() const { return directionalLightSRVHandle_; }

	/// @brief ポイントライトSRVハンドルを取得
	/// @return SRVのGPUハンドル
	D3D12_GPU_DESCRIPTOR_HANDLE GetPointLightSRV() const { return pointLightSRVHandle_; }

	/// @brief スポットライトSRVハンドルを取得
	/// @return SRVのGPUハンドル
	D3D12_GPU_DESCRIPTOR_HANDLE GetSpotLightSRV() const { return spotLightSRVHandle_; }

	/// @brief エリアライトSRVハンドルを取得
	/// @return SRVのGPUハンドル
	D3D12_GPU_DESCRIPTOR_HANDLE GetAreaLightSRV() const { return areaLightSRVHandle_; }

	/// @brief ライトカウントリソースを取得
	/// @return ライトカウントリソース
	ID3D12Resource* GetLightCountResource() const { return lightCountResource_.Get(); }

private:
	// 構造化バッファリソース
	ComPtr<ID3D12Resource> directionalLightBuffer_ = nullptr;
	ComPtr<ID3D12Resource> pointLightBuffer_ = nullptr;
	ComPtr<ID3D12Resource> spotLightBuffer_ = nullptr;
	ComPtr<ID3D12Resource> areaLightBuffer_ = nullptr;
	ComPtr<ID3D12Resource> lightCountResource_ = nullptr;

	// マップ済みポインタ
	DirectionalLightData* directionalLightData_ = nullptr;
	PointLightData* pointLightData_ = nullptr;
	SpotLightData* spotLightData_ = nullptr;
	AreaLightData* areaLightData_ = nullptr;
	LightCountData* lightCountData_ = nullptr;

	// SRVハンドル
	D3D12_GPU_DESCRIPTOR_HANDLE directionalLightSRVHandle_{};
	D3D12_GPU_DESCRIPTOR_HANDLE pointLightSRVHandle_{};
	D3D12_GPU_DESCRIPTOR_HANDLE spotLightSRVHandle_{};
	D3D12_GPU_DESCRIPTOR_HANDLE areaLightSRVHandle_{};

	// 最大ライト数
	uint32_t maxDirectionalLights_ = 0;
	uint32_t maxPointLights_ = 0;
	uint32_t maxSpotLights_ = 0;
	uint32_t maxAreaLights_ = 0;

	// 現在のライト数
	uint32_t currentDirectionalLightCount_ = 0;
	uint32_t currentPointLightCount_ = 0;
	uint32_t currentSpotLightCount_ = 0;
	uint32_t currentAreaLightCount_ = 0;

	/// @brief 構造化バッファとSRVを作成
	/// @param buffer バッファのポインタ
	/// @param srvHandle SRVハンドルの参照
	/// @param elementSize 要素のサイズ
	/// @param elementCount 要素数
	void CreateStructuredBuffer(ComPtr<ID3D12Resource>& buffer, D3D12_GPU_DESCRIPTOR_HANDLE& srvHandle, uint32_t elementSize, uint32_t elementCount);
};
}
