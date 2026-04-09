#pragma once
#include "ReflectionValidationState.h"

namespace GameEngine {

class PSOManager;
class ShaderManager;

class ReflectionValidationCoordinator {
public:
   void BeginFrame(ShaderManager* shaderManager, ReflectionValidationState& state) const;
   void EndFrame(ShaderManager* shaderManager, ReflectionValidationState& state) const;
   void UpdateValidationReport(PSOManager* psoManager, ShaderManager* shaderManager, ReflectionValidationState& state) const;

#ifdef USE_IMGUI
   void DrawDebugWindow(PSOManager* psoManager, ShaderManager* shaderManager, ReflectionValidationState& state) const;
#endif
};

}
