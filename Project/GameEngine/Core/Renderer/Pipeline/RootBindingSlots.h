#pragma once

namespace GameEngine {
namespace RootBindingSlots {
namespace Object3D {
constexpr unsigned int kMaterial = 0;
constexpr unsigned int kTransform = 1;
constexpr unsigned int kCamera = 2;
constexpr unsigned int kLightCount = 3;
constexpr unsigned int kDirectionalLight = 4;
constexpr unsigned int kPointLight = 5;
constexpr unsigned int kSpotLight = 6;
constexpr unsigned int kAreaLight = 7;
constexpr unsigned int kTexture = 8;
constexpr unsigned int kEnvMap = 9;
constexpr unsigned int kSkinPalette = 10; // スキニング時はenvmapの後
} // namespace Object3D

namespace Particle {
constexpr unsigned int kMaterial = 0;
constexpr unsigned int kInstancing = 1;
constexpr unsigned int kTexture = 2;
} // namespace Particle

namespace Line3D {
constexpr unsigned int kTransform = 0;
} // namespace Line3D

namespace FullscreenTriangle {
constexpr unsigned int kTexture = 0;
} // namespace FullscreenTriangle

namespace PostProcess {
constexpr unsigned int kConstantBuffer = 0;
constexpr unsigned int kInputTexture = 1;
} // namespace PostProcess

namespace Skybox {
constexpr unsigned int kMaterial = 0;
constexpr unsigned int kTransform = 1;
constexpr unsigned int kTexture = 2;
} // namespace Skybox
} // namespace RootBindingSlots
} // namespace GameEngine
