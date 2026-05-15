#pragma once
#include "IRenderPass.h"

namespace GameEngine {

/// @brief 不透明オブジェクト描画パス
/// FrameContext の opaqueCommands を順番に実行する。
class OpaquePass final : public IRenderPass {
public:
	void Execute(FrameContext& ctx) override;
	std::string_view GetName() const override { return "OpaquePass"; }
	RenderPassType GetPassType() const override { return RenderPassType::Opaque; }
};

} // namespace GameEngine
