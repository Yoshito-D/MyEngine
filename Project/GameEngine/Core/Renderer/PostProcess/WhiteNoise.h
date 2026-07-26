#pragma once
#include "PostProcess.h"
#include <chrono>
#include <wrl.h>
#include <d3d12.h>

namespace GameEngine {
/// @brief Runtime parameters for animated white noise.
struct WhiteNoiseParams {
   /// @brief Time used as the shader random seed source.
   float time = 0.0f;

   /// @brief Number of procedural noise cells across UV space.
   float noiseDensity = 320.0f;

   /// @brief Number of random pattern changes per second.
   float seedChangeRate = 24.0f;

   /// @brief Random threshold that decides whether a cell becomes noise.
   float noiseThreshold = 0.94f;

   /// @brief Strength multiplier for selected noise cells.
   float noiseIntensity = 0.85f;
};

/// @brief Multiplies the offscreen texture by animated white noise.
class WhiteNoise : public PostProcess {
public:
   /// @brief White noise constant buffer layout.
   struct WhiteNoiseCB {
	  /// @brief Time used as the shader random seed source.
	  float time;

	  /// @brief Number of procedural noise cells across UV space.
	  float noiseDensity;

	  /// @brief Number of random pattern changes per second.
	  float seedChangeRate;

	  /// @brief Random threshold that decides whether a cell becomes noise.
	  float noiseThreshold;

	  /// @brief Strength multiplier for selected noise cells.
	  float noiseIntensity;

	  /// @brief Padding for constant-buffer register alignment.
	  float padding[3];
   };

   /// @brief Initializes GPU resources used by the white noise effect.
   /// @param device Graphics device.
   /// @param renderTarget Offscreen render target.
   void Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) override;

   /// @brief Applies the effect to the supplied input texture.
   /// @param inputSRV Input texture SRV.
   void Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) override;

#ifdef USE_IMGUI
   /// @brief Shows debug controls for the effect parameters.
   void ImGuiEdit() override;
#endif

   /// @brief Gets the editor/debug display name.
   /// @return Effect name.
   const char* GetEffectName() const override { return "White Noise"; }

   /// @copydoc PostProcess::SerializeSettings
   nlohmann::json SerializeSettings() const override;

   /// @copydoc PostProcess::DeserializeSettings
   bool DeserializeSettings(const nlohmann::json& settings) override;

   /// @brief Sets all white noise parameters.
   /// @param params Parameters to apply.
   void SetParams(const WhiteNoiseParams& params);

   /// @brief Gets the current white noise parameters.
   /// @return Current parameters.
   const WhiteNoiseParams& GetParams() const { return params_; }

   /// @brief Sets the time passed to the black noise shader.
   /// @param time Time in seconds.
   void SetTime(float time);

   /// @brief Gets the time passed to the black noise shader.
   /// @return Time in seconds.
   float GetTime() const { return params_.time; }

   /// @brief Sets the procedural noise density.
   /// @param value Number of noise cells across UV space.
   void SetNoiseDensity(float value) { auto params = params_; params.noiseDensity = value; SetParams(params); }

   /// @brief Sets the seed change rate.
   /// @param value Number of random pattern changes per second.
   void SetSeedChangeRate(float value) { auto params = params_; params.seedChangeRate = value; SetParams(params); }

   /// @brief Sets the threshold used to choose noise cells.
   /// @param value Threshold in the range 0.0 to 1.0.
   void SetNoiseThreshold(float value) { auto params = params_; params.noiseThreshold = value; SetParams(params); }

   /// @brief Sets noise strength.
   /// @param value Strength multiplier in the range 0.0 to 1.0.
   void SetNoiseIntensity(float value) { auto params = params_; params.noiseIntensity = value; SetParams(params); }

private:
   WhiteNoiseParams params_;

   Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
   WhiteNoiseCB* constantBufferData_ = nullptr;
   std::chrono::steady_clock::time_point previousTime_;
   bool hasPreviousTime_ = false;

   void CreateConstantBuffer();
   void UpdateConstantBuffer();
   void AdvanceTime();
};
}
