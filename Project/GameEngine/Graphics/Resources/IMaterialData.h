#pragma once
#include <d3d12.h>
#include <string>

namespace GameEngine {

/// @brief マテリアルデータの共通インターフェース
/// Material と ParticleMaterial の共通基底として使用する
class IMaterialData {
public:
	virtual ~IMaterialData() = default;

	/// @brief GPU 送信用マテリアルリソースを取得
	virtual ID3D12Resource* GetMaterialResource() const = 0;

	/// @brief このマテリアルが使用するパイプライン名を取得
	/// @return パイプライン名（空文字列 = 各レンダラーのデフォルト動作）
	virtual const std::string& GetPipelineName() const = 0;
};

} // namespace GameEngine
