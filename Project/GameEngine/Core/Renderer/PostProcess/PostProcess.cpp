#include "pch.h"
#include "PostProcess.h"

namespace GameEngine {

void PostProcess::Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) {
   device_ = device;
   renderTarget_ = renderTarget;
}

void PostProcess::SetPipeline(PipelineState* pipeline, RootSignature* rootSignature) {
   pipeline_ = pipeline;
   rootSignature_ = rootSignature;
}

}