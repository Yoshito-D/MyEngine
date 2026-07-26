#pragma once
#include "IRenderPass.h"
#include "DrawCommand.h"
#include <vector>
#include <memory>

namespace GameEngine {

/// @brief 半透明オブジェクト描画パス
/// カメラ距離でソートしてから transparentCommands を実行する。
class TransparentPass final : public IRenderPass {
public:
	/// @copydoc IRenderPass::Execute
	void Execute(FrameContext& ctx) override;
	/// @copydoc IRenderPass::GetName
	std::string_view GetName() const override { return "TransparentPass"; }
	/// @copydoc IRenderPass::GetPassType
	RenderPassType GetPassType() const override { return RenderPassType::Transparent; }

private:
	/// @brief カメラ距離でコマンドをソート（遠い順）
	void Sort(std::vector<std::unique_ptr<IDrawCommand>>& commands) const;
};

} // namespace GameEngine
