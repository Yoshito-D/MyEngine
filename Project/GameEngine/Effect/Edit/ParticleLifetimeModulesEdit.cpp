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
        if (ImGui::Checkbox("Enabled (有効)##VelocityOverLifetime", &enabled)) {
            module->SetEnabled(enabled);
        }

        if (enabled) {
            Vector3 linearVelocity = module->GetLinearVelocity();
            if (ImGui::DragFloat3("Linear Velocity (線形速度)", &linearVelocity.x, 0.1f, -50.0f, 50.0f)) {
                module->SetLinearVelocity(linearVelocity);
            }

            float speedModifier = module->GetSpeedModifier();
            if (ImGui::DragFloat("Speed Modifier (速度補正)", &speedModifier, 0.01f, 0.0f, 5.0f)) {
                module->SetSpeedModifier(speedModifier);
            }
        }
#endif
    }

    void EditLimitVelocityModule(GameEngine::LimitVelocityOverLifetimeModule* module) {
#ifdef USE_IMGUI
        if (!module) return;

        bool enabled = module->IsEnabled();
        if (ImGui::Checkbox("Enabled (有効)##LimitVelocity", &enabled)) {
            module->SetEnabled(enabled);
        }

        if (enabled) {
            float speedLimit = module->GetSpeedLimit();
            if (ImGui::DragFloat("Speed Limit (速度上限)", &speedLimit, 0.1f, 0.0f, 100.0f)) {
                module->SetSpeedLimit(speedLimit);
            }

            float dampen = module->GetDampen();
            if (ImGui::DragFloat("Dampen (減衰)", &dampen, 0.01f, 0.0f, 1.0f)) {
                module->SetDampen(dampen);
            }
        }
#endif
    }

    void EditForceOverLifetimeModule(GameEngine::ForceOverLifetimeModule* module) {
#ifdef USE_IMGUI
        if (!module) return;

        bool enabled = module->IsEnabled();
        if (ImGui::Checkbox("Enabled (有効)##ForceOverLifetime", &enabled)) {
            module->SetEnabled(enabled);
        }

        if (enabled) {
            Vector3 force = module->GetForce();
            if (ImGui::DragFloat3("Force (力)", &force.x, 0.1f, -50.0f, 50.0f)) {
                module->SetForce(force);
            }
        }
#endif
    }

    void EditColorOverLifetimeModule(GameEngine::ColorOverLifetimeModule* module) {
#ifdef USE_IMGUI
        if (!module) return;

        bool enabled = module->IsEnabled();
        if (ImGui::Checkbox("Enabled (有効)##ColorOverLifetime", &enabled)) {
            module->SetEnabled(enabled);
        }

        if (enabled) {
            Vector4 startColor = module->GetStartColor();
            if (ImGui::ColorEdit4("Start Color (開始色)##ColorLifetime", &startColor.x)) {
                module->SetStartColor(startColor);
            }

            Vector4 endColor = module->GetEndColor();
            if (ImGui::ColorEdit4("End Color (終了色)##ColorLifetime", &endColor.x)) {
                module->SetEndColor(endColor);
            }
        }
#endif
    }

    void EditSizeOverLifetimeModule(GameEngine::SizeOverLifetimeModule* module) {
#ifdef USE_IMGUI
        if (!module) return;

        bool enabled = module->IsEnabled();
        if (ImGui::Checkbox("Enabled (有効)##SizeOverLifetime", &enabled)) {
            module->SetEnabled(enabled);
        }

        if (enabled) {
            float sizeMultiplier = module->GetSizeMultiplier();
            if (ImGui::DragFloat("Size Multiplier (サイズ倍率)", &sizeMultiplier, 0.01f, 0.0f, 10.0f)) {
                module->SetSizeMultiplier(sizeMultiplier);
            }

            Vector3 startSize = module->GetStartSize();
            if (ImGui::DragFloat3("Start Size (開始サイズ)##SizeLifetime", &startSize.x, 0.01f, 0.0f, 10.0f)) {
                module->SetStartSize(startSize);
            }

            Vector3 endSize = module->GetEndSize();
            if (ImGui::DragFloat3("End Size (終了サイズ)##SizeLifetime", &endSize.x, 0.01f, 0.0f, 10.0f)) {
                module->SetEndSize(endSize);
            }
        }
#endif
    }

    void EditRotationOverLifetimeModule(GameEngine::RotationOverLifetimeModule* module) {
#ifdef USE_IMGUI
        if (!module) return;

        bool enabled = module->IsEnabled();
        if (ImGui::Checkbox("Enabled (有効)##RotationOverLifetime", &enabled)) {
            module->SetEnabled(enabled);
        }

        if (enabled) {
            ImGui::Text("Angular Velocity (角速度)");
            bool randomize = module->GetAngularVelocityRandomize();
            if (ImGui::Checkbox("Randomize (ランダム化)##AngularVelocity", &randomize)) {
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
        if (ImGui::Checkbox("Enabled (有効)##Noise", &enabled)) {
            module->SetEnabled(enabled);
        }

        if (enabled) {
            float strength = module->GetStrength();
            if (ImGui::DragFloat("Strength (強さ)", &strength, 0.1f, 0.0f, 10.0f)) {
                module->SetStrength(strength);
            }

            float frequency = module->GetFrequency();
            if (ImGui::DragFloat("Frequency (周波数)", &frequency, 0.01f, 0.0f, 5.0f)) {
                module->SetFrequency(frequency);
            }

            float scrollSpeed = module->GetScrollSpeed();
            if (ImGui::DragFloat("Scroll Speed (スクロール速度)", &scrollSpeed, 0.1f, 0.0f, 10.0f)) {
                module->SetScrollSpeed(scrollSpeed);
            }
        }
#endif
    }

}
