#pragma once
#include "PostProcess.h"

namespace GameEngine {
/// @brief グレースケール効果
class Grayscale : public PostProcess {
public:
   /// @brief エフェクトを適用
   /// @param inputSRV 入力SRV
   void Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) override;

#ifdef USE_IMGUI
   void ImGuiEdit() override;
#endif
   const char* GetEffectName() const override { return "Grayscale"; }
};
}