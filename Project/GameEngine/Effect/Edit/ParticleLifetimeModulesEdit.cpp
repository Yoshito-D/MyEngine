#include "pch.h"
#include "ParticleLifetimeModulesEdit.h"
#include "Effect/Module/LifetimeModules.h"
#include "Utility/VectorMath.h"

#ifdef USE_IMGUI
#include "../../../externals/imgui/imgui.h"
#include <string>
#endif

using namespace GameEngine;

namespace ParticleSystemEdit {

#ifdef USE_IMGUI
namespace {

bool EditRandomFloat(const char* label, const char* id, RandomFloat& value, float dragSpeed, float minValue, float maxValue) {
    bool changed = false;
    ImGui::PushID(id);
    ImGui::Text("%s", label);

    bool randomize = value.randomize;
    if (ImGui::Checkbox("Randomize", &randomize)) {
        value.randomize = randomize;
        changed = true;
    }

    if (value.randomize) {
        float min = value.minValue;
        float max = value.maxValue;
        if (ImGui::DragFloat("Min", &min, dragSpeed, minValue, maxValue)) {
            if (min > max) min = max;
            value.minValue = min;
            changed = true;
        }
        if (ImGui::DragFloat("Max", &max, dragSpeed, minValue, maxValue)) {
            if (max < min) max = min;
            value.maxValue = max;
            changed = true;
        }
    } else {
        float scalar = value.minValue;
        if (ImGui::DragFloat("Value", &scalar, dragSpeed, minValue, maxValue)) {
            value.minValue = scalar;
            value.maxValue = scalar;
            changed = true;
        }
    }

    ImGui::PopID();
    return changed;
}

bool EditRandomVector3(const char* label, const char* id, RandomVector3& value, float dragSpeed, float minValue, float maxValue) {
    bool changed = false;
    ImGui::PushID(id);
    ImGui::Text("%s", label);

    bool randomize = value.randomize;
    if (ImGui::Checkbox("Randomize", &randomize)) {
        value.randomize = randomize;
        changed = true;
    }

    if (value.randomize) {
        float minArr[3] = { value.minValue.x, value.minValue.y, value.minValue.z };
        float maxArr[3] = { value.maxValue.x, value.maxValue.y, value.maxValue.z };

        if (ImGui::DragFloat3("Min", minArr, dragSpeed, minValue, maxValue)) {
            for (int i = 0; i < 3; ++i) {
                if (minArr[i] > maxArr[i]) minArr[i] = maxArr[i];
            }
            value.minValue = Vector3(minArr[0], minArr[1], minArr[2]);
            changed = true;
        }
        if (ImGui::DragFloat3("Max", maxArr, dragSpeed, minValue, maxValue)) {
            for (int i = 0; i < 3; ++i) {
                if (maxArr[i] < minArr[i]) maxArr[i] = minArr[i];
            }
            value.maxValue = Vector3(maxArr[0], maxArr[1], maxArr[2]);
            changed = true;
        }
    } else {
        float scalar[3] = { value.minValue.x, value.minValue.y, value.minValue.z };
        if (ImGui::DragFloat3("Value", scalar, dragSpeed, minValue, maxValue)) {
            Vector3 vectorValue(scalar[0], scalar[1], scalar[2]);
            value.minValue = vectorValue;
            value.maxValue = vectorValue;
            changed = true;
        }
    }

    ImGui::PopID();
    return changed;
}

} // namespace
#endif

    void EditVelocityOverLifetimeModule(GameEngine::VelocityOverLifetimeModule* module) {
#ifdef USE_IMGUI
        if (!module) return;

        bool enabled = module->IsEnabled();
        if (ImGui::Checkbox("Enabled (有効)##VelocityOverLifetime", &enabled)) {
            module->SetEnabled(enabled);
        }

        if (enabled) {
            RandomVector3 linearVelocity = module->GetLinearVelocityRange();
            if (EditRandomVector3("Linear Velocity (線形速度)", "VelocityLinear", linearVelocity, 0.1f, -50.0f, 50.0f)) {
                module->SetLinearVelocityRange(linearVelocity);
            }

            RandomFloat speedModifier = module->GetSpeedModifierRange();
            if (EditRandomFloat("Speed Modifier (速度補正)", "VelocitySpeedModifier", speedModifier, 0.01f, 0.0f, 5.0f)) {
                module->SetSpeedModifierRange(speedModifier);
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
            RandomFloat speedLimit = module->GetSpeedLimitRange();
            if (EditRandomFloat("Speed Limit (速度上限)", "LimitSpeed", speedLimit, 0.1f, 0.0f, 100.0f)) {
                module->SetSpeedLimitRange(speedLimit);
            }

            RandomFloat dampen = module->GetDampenRange();
            if (EditRandomFloat("Dampen (減衰)", "LimitDampen", dampen, 0.01f, 0.0f, 1.0f)) {
                module->SetDampenRange(dampen);
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
            RandomVector3 force = module->GetForceRange();
            if (EditRandomVector3("Force (力)", "Force", force, 0.1f, -50.0f, 50.0f)) {
                module->SetForceRange(force);
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
            RandomFloat strength = module->GetStrengthRange();
            if (EditRandomFloat("Strength (強さ)", "NoiseStrength", strength, 0.1f, 0.0f, 10.0f)) {
                module->SetStrengthRange(strength);
            }

            RandomFloat frequency = module->GetFrequencyRange();
            if (EditRandomFloat("Frequency (周波数)", "NoiseFrequency", frequency, 0.01f, 0.0f, 5.0f)) {
                module->SetFrequencyRange(frequency);
            }

            RandomFloat scrollSpeed = module->GetScrollSpeedRange();
            if (EditRandomFloat("Scroll Speed (スクロール速度)", "NoiseScrollSpeed", scrollSpeed, 0.1f, 0.0f, 10.0f)) {
                module->SetScrollSpeedRange(scrollSpeed);
            }
        }
#endif
    }

}
