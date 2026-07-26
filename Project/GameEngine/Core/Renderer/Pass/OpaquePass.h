#pragma once
#include "IRenderPass.h"

namespace GameEngine {

/// @brief 不透明オブジェクト描画パス
/// FrameContext の opaqueCommands を順番に実行する。
class OpaquePass final : public IRenderPass {
public:
	/// @copydoc IRenderPass::Execute
	void Execute(FrameContext& ctx) override;
	/// @copydoc IRenderPass::GetName
	std::string_view GetName() const override { return "OpaquePass"; }
	/// @copydoc IRenderPass::GetPassType
	RenderPassType GetPassType() const override { return RenderPassType::Opaque; }
};

} // namespace GameEngine
