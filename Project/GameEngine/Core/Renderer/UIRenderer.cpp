#include "pch.h"
#include "UIRenderer.h"
#include "SpriteRenderer.h"
#include "Graphics/GraphicsDevice.h"
#include "PSOManager.h"
#include "Camera/Camera.h"
#include "Sprite/Sprite.h"
#include "Graphics/Texture.h"

namespace GameEngine {

void UIRenderer::Initialize(GraphicsDevice* device, PSOManager* psoManager, Camera* uiCamera, SpriteRenderer* spriteRenderer, LightManager* lightManager) {
	device_ = device;
	psoManager_ = psoManager;
	uiCamera_ = uiCamera;
	spriteRenderer_ = spriteRenderer;
	lightManager_ = lightManager;
}

void UIRenderer::DrawUISprite(const UISpriteDrawData& uiSpriteData,
	Material* defaultMaterial,
	std::function<void(const std::string&, BlendMode)> setPipelineFunc) {

	Sprite* sprite = uiSpriteData.sprite;
	assert(sprite != nullptr);
	assert(uiSpriteData.texture != nullptr);

	SpriteDrawData spriteData;
	spriteData.sprite = sprite;
	spriteData.texture = uiSpriteData.texture;
	spriteData.textureSrvHandle = uiSpriteData.textureSrvHandle;
	spriteData.camera = uiCamera_;
	spriteData.blendMode = uiSpriteData.blendMode;

	// SpriteRendererに委譲
	spriteRenderer_->DrawSprite(spriteData, defaultMaterial, lightManager_, setPipelineFunc);
}

} // namespace GameEngine
