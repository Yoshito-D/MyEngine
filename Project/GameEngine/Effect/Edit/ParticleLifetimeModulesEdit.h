#pragma once

namespace GameEngine {
class VelocityOverLifetimeModule;
class LimitVelocityOverLifetimeModule;
class ForceOverLifetimeModule;
class ColorOverLifetimeModule;
class SizeOverLifetimeModule;
class RotationOverLifetimeModule;
class NoiseModule;
}

namespace ParticleSystemEdit {
/// @brief Velocity over Lifetime Moduleの編集UI
void EditVelocityOverLifetimeModule(GameEngine::VelocityOverLifetimeModule* module);

/// @brief Limit Velocity over Lifetime Moduleの編集UI
void EditLimitVelocityModule(GameEngine::LimitVelocityOverLifetimeModule* module);

/// @brief Force over Lifetime Moduleの編集UI
void EditForceOverLifetimeModule(GameEngine::ForceOverLifetimeModule* module);

/// @brief Color over Lifetime Moduleの編集UI
void EditColorOverLifetimeModule(GameEngine::ColorOverLifetimeModule* module);

/// @brief Size over Lifetime Moduleの編集UI
void EditSizeOverLifetimeModule(GameEngine::SizeOverLifetimeModule* module);

/// @brief Rotation over Lifetime Moduleの編集UI
void EditRotationOverLifetimeModule(GameEngine::RotationOverLifetimeModule* module);

/// @brief Noise Moduleの編集UI
void EditNoiseModule(GameEngine::NoiseModule* module);
}
