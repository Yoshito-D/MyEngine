#include "pch.h"
#include "PostProcess.h"
#ifdef USE_IMGUI
#include "Utility/ImGuiHelper.h"
#endif
#include <nlohmann/json.hpp>

namespace GameEngine {

void PostProcess::Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) {
   device_ = device;
   renderTarget_ = renderTarget;
}

void PostProcess::SetPipeline(PipelineState* pipeline, RootSignature* rootSignature) {
   pipeline_ = pipeline;
   rootSignature_ = rootSignature;
}

void PostProcess::SetBindingSlots(UINT constantBufferSlot, UINT inputTextureSlot) {
   constantBufferRootSlot_ = constantBufferSlot;
   inputTextureRootSlot_ = inputTextureSlot;
}

void PostProcess::SetDepthTextureRootSlot(UINT depthTextureSlot) {
   depthTextureRootSlot_ = depthTextureSlot;
}

void PostProcess::SetMaskTextureRootSlot(UINT maskTextureSlot) {
   maskTextureRootSlot_ = maskTextureSlot;
}

nlohmann::json PostProcess::SerializeSettings() const {
   return nlohmann::json::object();
}

bool PostProcess::DeserializeSettings(const nlohmann::json& settings) {
   return settings.is_object();
}

#ifdef USE_IMGUI
const char* PostProcess::LocalizeEditorText(const char* japanese, const char* english) {
   return ImGuiHelper::Localize({ japanese, english });
}
#endif

}
