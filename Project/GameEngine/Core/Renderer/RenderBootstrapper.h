#pragma once

namespace GameEngine {

class GraphicsDevice;
class Window;
class Camera;
class AssetManager;
class CameraManager;
class LightManager;
class Material;
class ShaderManager;
class PSOManager;
class ModelRenderer;
class SpriteRenderer;
class ParticleRenderer;
class UIRenderer;
class OffscreenRenderTarget;
class LineRenderer;
class PostProcessManager;

struct RenderBootstrapContext {
   GraphicsDevice* device = nullptr;
   Window* window = nullptr;
   CameraManager* cameraManager = nullptr;
   LightManager* lightManager = nullptr;
   AssetManager* assetManager = nullptr;

   Material* defaultMaterial = nullptr;
   OffscreenRenderTarget* offscreenRenderTarget = nullptr;
   ShaderManager* shaderManager = nullptr;
   PSOManager* psoManager = nullptr;
   ModelRenderer* modelRenderer = nullptr;
   SpriteRenderer* spriteRenderer = nullptr;
   ParticleRenderer* particleRenderer = nullptr;
   UIRenderer* uiRenderer = nullptr;
   LineRenderer* lineRenderer = nullptr;
   LineRenderer* postProcessLineRenderer = nullptr;
   Camera* uiCamera = nullptr;
   PostProcessManager* postProcessManager = nullptr;
};

class RenderBootstrapper {
public:
   void Initialize(const RenderBootstrapContext& context) const;
};

}
