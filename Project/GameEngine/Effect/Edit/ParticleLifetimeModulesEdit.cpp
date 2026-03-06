#include "pch.h"
#include "ParticleLifetimeModulesEdit.h"
#include "Effect/Module/LifetimeModules.h"
#include "Utility/VectorMath.h"

#ifdef USE_IMGUI
#include "../../../externals/imgui/imgui.h"
#endif

using namespace GameEngine;

namespace ParticleSystemEdit {

    void EditVelocityOverLifetimeModule(GameEngine::VelocityOverLifetimeModule* module) {
#ifdef USE_IMGUI
        if (!module) return;

        bool enabled = module->IsEnabled();
        if (ImGui::Checkbox("Enabled##VelocityOverLifetime", &enabled)) {
            module->SetEnabled(enabled);
        }

        if (enabled) {
            Vector3 linearVelocity = module->GetLinearVelocity();
            if (ImGui::DragFloat3("Linear Velocity", &linearVelocity.x, 0.1f, -50.0f, 50.0f)) {
                module->SetLinearVelocity(linearVelocity);
            }

            float speedModifier = module->GetSpeedModifier();
            if (ImGui::DragFloat("Speed Modifier", &speedModifier, 0.01f, 0.0f, 5.0f)) {
                module->SetSpeedModifier(speedModifier);
            }
        }
#endif
    }

    void EditLimitVelocityModule(GameEngine::LimitVelocityOverLifetimeModule* module) {
#ifdef USE_IMGUI
        if (!module) return;

        bool enabled = module->IsEnabled();
        if (ImGui::Checkbox("Enabled##LimitVelocity", &enabled)) {
            module->SetEnabled(enabled);
        }

        if (enabled) {
            float speedLimit = module->GetSpeedLimit();
            if (ImGui::DragFloat("Speed Limit", &speedLimit, 0.1f, 0.0f, 100.0f)) {
                module->SetSpeedLimit(speedLimit);
            }

            float dampen = module->GetDampen();
            if (ImGui::DragFloat("Dampen", &dampen, 0.01f, 0.0f, 1.0f)) {
                module->SetDampen(dampen);
            }
        }
#endif
    }

    void EditForceOverLifetimeModule(GameEngine::ForceOverLifetimeModule* module) {
#ifdef USE_IMGUI
        if (!module) return;

        bool enabled = module->IsEnabled();
        if (ImGui::Checkbox("Enabled##ForceOverLifetime", &enabled)) {
            module->SetEnabled(enabled);
        }

        if (enabled) {
            Vector3 force = module->GetForce();
            if (ImGui::DragFloat3("Force", &force.x, 0.1f, -50.0f, 50.0f)) {
                module->SetForce(force);
            }
        }
#endif
    }

    void EditColorOverLifetimeModule(GameEngine::ColorOverLifetimeModule* module) {
#ifdef USE_IMGUI
        if (!module) return;

        bool enabled = module->IsEnabled();
        if (ImGui::Checkbox("Enabled##ColorOverLifetime", &enabled)) {
            module->SetEnabled(enabled);
        }

        if (enabled) {
            Vector4 startColor = module->GetStartColor();
            if (ImGui::ColorEdit4("Start Color##ColorLifetime", &startColor.x)) {
                module->SetStartColor(startColor);
            }

            Vector4 endColor = module->GetEndColor();
            if (ImGui::ColorEdit4("End Color##ColorLifetime", &endColor.x)) {
                module->SetEndColor(endColor);
            }
        }
#endif
    }

    void EditSizeOverLifetimeModule(GameEngine::SizeOverLifetimeModule* module) {
#ifdef USE_IMGUI
        if (!module) return;

        bool enabled = module->IsEnabled();
        if (ImGui::Checkbox("Enabled##SizeOverLifetime", &enabled)) {
            module->SetEnabled(enabled);
        }

        if (enabled) {
            float sizeMultiplier = module->GetSizeMultiplier();
            if (ImGui::DragFloat("Size Multiplier", &sizeMultiplier, 0.01f, 0.0f, 10.0f)) {
                module->SetSizeMultiplier(sizeMultiplier);
            }

            float startSize = module->GetStartSize();
            if (ImGui::DragFloat("Start Size##SizeLifetime", &startSize, 0.01f, 0.0f, 10.0f)) {
                module->SetStartSize(startSize);
            }

            float endSize = module->GetEndSize();
            if (ImGui::DragFloat("End Size##SizeLifetime", &endSize, 0.01f, 0.0f, 10.0f)) {
                module->SetEndSize(endSize);
            }
        }
#endif
    }

    void EditRotationOverLifetimeModule(GameEngine::RotationOverLifetimeModule* module) {
#ifdef USE_IMGUI
        if (!module) return;

        bool enabled = module->IsEnabled();
        if (ImGui::Checkbox("Enabled##RotationOverLifetime", &enabled)) {
            module->SetEnabled(enabled);
        }

        if (enabled) {
            ImGui::Text("Angular Velocity");
            bool randomize = module->GetAngularVelocityRandomize();
            if (ImGui::Checkbox("Randomize##AngularVelocity", &randomize)) {
                module->SetAngularVelocityRandomize(randomize);
            }
            
            if (randomize) {
                Vector3 angularVelocityMin = module->GetAngularVelocityMin();
                Vector3 angularVelocityMax = module->GetAngularVelocityMax();
                
                float minArr[3] = {angularVelocityMin.x, angularVelocityMin.y, angularVelocityMin.z};
                float maxArr[3] = {angularVelocityMax.x, angularVelocityMax.y, angularVelocityMax.z};
                
                if (ImGui::DragFloat3("Min##AngularVelocity", minArr, 1.0f, -360.0f, 360.0f)) {
                    // 各軸でMinがMaxを超えないように制約
                    for (int i = 0; i < 3; ++i) {
                        if (minArr[i] > maxArr[i]) {
                            minArr[i] = maxArr[i];
                        }
                    }
                    module->SetAngularVelocityMin(Vector3(minArr[0], minArr[1], minArr[2]));
                }
                if (ImGui::DragFloat3("Max##AngularVelocity", maxArr, 1.0f, -360.0f, 360.0f)) {
                    // 各軸でMaxがMinを下回らないように制約
                    for (int i = 0; i < 3; ++i) {
                        if (maxArr[i] < minArr[i]) {
                            maxArr[i] = minArr[i];
                        }
                    }
                    module->SetAngularVelocityMax(Vector3(maxArr[0], maxArr[1], maxArr[2]));
                }
            } else {
                Vector3 angularVelocity = module->GetAngularVelocityMin();
                float value[3] = {angularVelocity.x, angularVelocity.y, angularVelocity.z};
                
                if (ImGui::DragFloat3("Value##AngularVelocity", value, 1.0f, -360.0f, 360.0f)) {
                    Vector3 vel(value[0], value[1], value[2]);
                    module->SetAngularVelocityMin(vel);
                    module->SetAngularVelocityMax(vel);
                }
            }
        }
#endif
    }

    void EditNoiseModule(GameEngine::NoiseModule* module) {
#ifdef USE_IMGUI
        if (!module) return;

        bool enabled = module->IsEnabled();
        if (ImGui::Checkbox("Enabled##Noise", &enabled)) {
            module->SetEnabled(enabled);
        }

        if (enabled) {
            float strength = module->GetStrength();
            if (ImGui::DragFloat("Strength", &strength, 0.1f, 0.0f, 10.0f)) {
                module->SetStrength(strength);
            }

            float frequency = module->GetFrequency();
            if (ImGui::DragFloat("Frequency", &frequency, 0.01f, 0.0f, 5.0f)) {
                module->SetFrequency(frequency);
            }

            float scrollSpeed = module->GetScrollSpeed();
            if (ImGui::DragFloat("Scroll Speed", &scrollSpeed, 0.1f, 0.0f, 10.0f)) {
                module->SetScrollSpeed(scrollSpeed);
            }
        }
#endif
    }

}
