#include "pch.h"
#include "OpaquePass.h"
#include "FrameContext.h"
#include "CommandDispatch.h"

namespace GameEngine {

void OpaquePass::Execute(FrameContext& ctx) {
	if (!ctx.opaqueCommands) {
		return;
	}

	for (const auto& icmd : *ctx.opaqueCommands) {
		DispatchDrawCommand(icmd->GetDrawCommand(), ctx);
	}
}

} // namespace GameEngine
