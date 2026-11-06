#pragma once

#include "GameEngine/Graphics/PostProcess/PostProcess.h"

namespace GameEngine {

/// @brief リニア空間からsRGB空間への変換エフェクト
class LinearToSRGB : public PostProcess {
public:
   /// @brief エフェクトを適用
   /// @param inputSRV 入力SRV
   void Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) override;

#ifdef USE_IMGUI

   /// @brief リニア色からsRGBへの変換効果の項目見出しを表示する。固有の編集パラメータは持たない。
   void ImGuiEdit() override;
#endif

   /// @brief エディター表示と保存識別に使うエフェクト名を返す。
   /// @return 「Linear to sRGB」を表す固定文字列。
   const char* GetEffectName() const override { return "Linear to sRGB"; }
};
}
