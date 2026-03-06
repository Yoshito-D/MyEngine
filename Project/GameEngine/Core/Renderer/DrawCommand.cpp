#include "pch.h"
#include "DrawCommand.h"

namespace GameEngine {

DrawCommand DrawCommand::CreateModel(Model* model, const std::vector<D3D12_GPU_DESCRIPTOR_HANDLE>& textures,
                                    Camera* camera, BlendMode blendMode, RenderPass renderPass) {
    DrawCommand cmd;
    cmd.type = DrawCommandType::Model;
    cmd.blendMode = blendMode;
    cmd.renderPass = renderPass;
    cmd.modelData.model = model;
    cmd.modelData.textures = textures;
    cmd.modelData.camera = camera;
    cmd.modelData.blendMode = blendMode;
    return cmd;
}

DrawCommand DrawCommand::CreateSprite(Sprite* sprite, Texture* texture, D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle,
                                     Camera* camera, BlendMode blendMode, RenderPass renderPass) {
    DrawCommand cmd;
    cmd.type = DrawCommandType::Sprite;
    cmd.blendMode = blendMode;
    cmd.renderPass = renderPass;
    cmd.spriteData.sprite = sprite;
    cmd.spriteData.texture = texture;
    cmd.spriteData.textureSrvHandle = textureSrvHandle;
    cmd.spriteData.camera = camera;
    cmd.spriteData.blendMode = blendMode;
    return cmd;
}

DrawCommand DrawCommand::CreateUISprite(Sprite* sprite, Texture* texture, D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle,
                                       Sprite::AnchorPoint anchorPoint, uint32_t screenWidth, uint32_t screenHeight,
                                       BlendMode blendMode, RenderPass renderPass) {
    DrawCommand cmd;
    cmd.type = DrawCommandType::Sprite;
    cmd.blendMode = blendMode;
    cmd.renderPass = renderPass;
    cmd.isUISprite = true;  // UIスプライトフラグを設定
    cmd.uiSpriteData.sprite = sprite;
    cmd.uiSpriteData.texture = texture;
    cmd.uiSpriteData.textureSrvHandle = textureSrvHandle;
    cmd.uiSpriteData.anchorPoint = anchorPoint;
    cmd.uiSpriteData.screenWidth = screenWidth;
    cmd.uiSpriteData.screenHeight = screenHeight;
    cmd.uiSpriteData.blendMode = blendMode;
    return cmd;
}

DrawCommand DrawCommand::CreateParticle(ParticleSystem* particleSystem, Camera* camera,
                                       BlendMode blendMode, RenderPass renderPass) {
    DrawCommand cmd;
    cmd.type = DrawCommandType::Particle;
    cmd.blendMode = blendMode;
    cmd.renderPass = renderPass;
    cmd.particleData.particleSystem = particleSystem;
    cmd.particleData.camera = camera;
    return cmd;
}

DrawCommand DrawCommand::CreateLine(std::function<void(ID3D12GraphicsCommandList*, const Matrix4x4&)> drawFunc, Camera* camera,
                                   RenderPass renderPass) {
    DrawCommand cmd;
    cmd.type = DrawCommandType::Line;
    cmd.blendMode = BlendMode::kBlendModeNormal;
    cmd.renderPass = renderPass;
    cmd.lineData.drawFunc = drawFunc;
    cmd.lineData.camera = camera;
    if (camera) {
        cmd.lineData.viewProjectionMatrix = camera->GetViewProjectionMatrix();
    }
    return cmd;
}

} // namespace GameEngine
