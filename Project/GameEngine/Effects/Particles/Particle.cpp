#include "GameEngine/pch.h"
#include "GameEngine/Effects/Particles/Particle.h"

namespace GameEngine {

Particle::Particle()
   : simulationSpaceTransform(), velocity(0.0f, 0.0f, 0.0f), acceleration(0.0f, 0.0f, 0.0f),
     angularVelocity(0.0f, 0.0f, 0.0f), velocityOverLifetimeLinearVelocity(0.0f, 0.0f, 0.0f),
     velocityOverLifetimeSpeedModifier(1.0f), forceOverLifetimeForce(0.0f, 0.0f, 0.0f), limitVelocitySpeedLimit(10.0f),
     limitVelocityDampen(0.5f), noiseStrength(1.0f), noiseFrequency(0.5f), noiseScrollSpeed(1.0f), noiseTime(0.0f),
     color(1.0f, 1.0f, 1.0f, 1.0f), customData(0.0f, 0.0f, 1.0f, 0.0f), ribbonWidth(0.5f), subEmitterTimer(0.0f),
     gravityModifier(0.0f), drag(0.0f), sizeOverLifetimeMultiplier(1.0f), lifeTime(1.0f), currentTime(0.0f),
     initialSize(1.0f, 1.0f, 1.0f), currentSize(1.0f, 1.0f, 1.0f), uvOffset(0.0f, 0.0f), uvScale(1.0f, 1.0f),
     uvRotation(0.0f), uvScroll(0.0f, 0.0f), sheetFrame(0), sheetRow(0), isActive(false) {
}

float Particle::GetLifeProgress() const {
   if (lifeTime <= 0.0f) {
      return 1.0f;
   }

   return currentTime / lifeTime;
}

} // namespace GameEngine
