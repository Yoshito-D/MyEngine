#include "pch.h"
#include "ShapeModule.h"
#include <random>
#include <cmath>

namespace GameEngine {
    namespace {
        std::random_device randomDevice;
        std::mt19937 randomEngine(randomDevice());

        float RandomRange(float min, float max) {
            std::uniform_real_distribution<float> dist(min, max);
            return dist(randomEngine);
        }

        constexpr float kPi = 3.14159265358979323846f;
    }

    ShapeModule::ShapeModule() = default;

    Vector3 ShapeModule::GetRandomEmissionPosition() const {
        Vector3 position = position_;

        switch (shapeType_) {
            case ShapeType::Sphere: {
                float theta = RandomRange(0.0f, 2.0f * kPi);
                float phi = RandomRange(0.0f, kPi);
                float r = (emitFrom_ == EmitFrom::Shell) ? radius_ : RandomRange(0.0f, radius_);
                position.x += r * std::sin(phi) * std::cos(theta) * scale_.x;
                position.y += r * std::cos(phi) * scale_.y;
                position.z += r * std::sin(phi) * std::sin(theta) * scale_.z;
                break;
            }
            case ShapeType::Hemisphere: {
                float theta = RandomRange(0.0f, 2.0f * kPi);
                float phi = RandomRange(0.0f, kPi * 0.5f);
                float r = (emitFrom_ == EmitFrom::Shell) ? radius_ : RandomRange(0.0f, radius_);
                position.x += r * std::sin(phi) * std::cos(theta) * scale_.x;
                position.y += r * std::cos(phi) * scale_.y;
                position.z += r * std::sin(phi) * std::sin(theta) * scale_.z;
                break;
            }
            case ShapeType::Cone: {
                float angle = RandomRange(0.0f, angle_ * kPi / 180.0f);
                float rotation = RandomRange(0.0f, 2.0f * kPi);
                float distance = RandomRange(0.0f, length_);
                position.x += distance * std::sin(angle) * std::cos(rotation) * scale_.x;
                position.y += distance * std::cos(angle) * scale_.y;
                position.z += distance * std::sin(angle) * std::sin(rotation) * scale_.z;
                break;
            }
            case ShapeType::Box: {
                if (emitFrom_ == EmitFrom::Volume) {
                    position.x += RandomRange(-boxSize_.x * 0.5f, boxSize_.x * 0.5f) * scale_.x;
                    position.y += RandomRange(-boxSize_.y * 0.5f, boxSize_.y * 0.5f) * scale_.y;
                    position.z += RandomRange(-boxSize_.z * 0.5f, boxSize_.z * 0.5f) * scale_.z;
                } else {
                    int face = static_cast<int>(RandomRange(0.0f, 6.0f));
                    switch (face) {
                        case 0: position.x += boxSize_.x * 0.5f * scale_.x; break;
                        case 1: position.x -= boxSize_.x * 0.5f * scale_.x; break;
                        case 2: position.y += boxSize_.y * 0.5f * scale_.y; break;
                        case 3: position.y -= boxSize_.y * 0.5f * scale_.y; break;
                        case 4: position.z += boxSize_.z * 0.5f * scale_.z; break;
                        case 5: position.z -= boxSize_.z * 0.5f * scale_.z; break;
                    }
                }
                break;
            }
            case ShapeType::Circle: {
                float angle = RandomRange(0.0f, arc_ * kPi / 180.0f);
                float r = (emitFrom_ == EmitFrom::Edge) ? radius_ : RandomRange(0.0f, radius_);
                position.x += r * std::cos(angle) * scale_.x;
                position.z += r * std::sin(angle) * scale_.z;
                break;
            }
            case ShapeType::Edge: {
                float t = RandomRange(0.0f, 1.0f);
                position.x += (t - 0.5f) * scale_.x;
                break;
            }
            case ShapeType::Point:
            default:
                break;
        }

        return position;
    }

    Vector3 ShapeModule::GetRandomEmissionDirection() const {
        Vector3 direction{0.0f, 1.0f, 0.0f};

        switch (shapeType_) {
            case ShapeType::Sphere:
            case ShapeType::Hemisphere: {
                float theta = RandomRange(0.0f, 2.0f * kPi);
                float phi = (shapeType_ == ShapeType::Hemisphere) 
                    ? RandomRange(0.0f, kPi * 0.5f) 
                    : RandomRange(0.0f, kPi);
                direction.x = std::sin(phi) * std::cos(theta);
                direction.y = std::cos(phi);
                direction.z = std::sin(phi) * std::sin(theta);
                break;
            }
            case ShapeType::Cone: {
                float angle = RandomRange(0.0f, angle_ * kPi / 180.0f);
                float rotation = RandomRange(0.0f, 2.0f * kPi);
                direction.x = std::sin(angle) * std::cos(rotation);
                direction.y = std::cos(angle);
                direction.z = std::sin(angle) * std::sin(rotation);
                break;
            }
            default:
                break;
        }

        return direction.Normalize();
    }

    nlohmann::json ShapeModule::ToJson() const {
        nlohmann::json j;
        
        j["enabled"] = enabled_;
        j["shapeType"] = static_cast<int>(shapeType_);
        j["emitFrom"] = static_cast<int>(emitFrom_);
        j["radius"] = radius_;
        j["angle"] = angle_;
        j["length"] = length_;
        j["boxSize"] = {boxSize_.x, boxSize_.y, boxSize_.z};
        j["arc"] = arc_;
        j["position"] = {position_.x, position_.y, position_.z};
        j["rotation"] = {rotation_.x, rotation_.y, rotation_.z};
        j["scale"] = {scale_.x, scale_.y, scale_.z};
        
        return j;
    }

    void ShapeModule::FromJson(const nlohmann::json& j) {
        if (j.contains("enabled")) enabled_ = j["enabled"];
        if (j.contains("shapeType")) shapeType_ = static_cast<ShapeType>(j["shapeType"].get<int>());
        if (j.contains("emitFrom")) emitFrom_ = static_cast<EmitFrom>(j["emitFrom"].get<int>());
        if (j.contains("radius")) radius_ = j["radius"];
        if (j.contains("angle")) angle_ = j["angle"];
        if (j.contains("length")) length_ = j["length"];
        
        if (j.contains("boxSize")) {
            auto arr = j["boxSize"];
            boxSize_ = Vector3{arr[0], arr[1], arr[2]};
        }
        
        if (j.contains("arc")) arc_ = j["arc"];
        
        if (j.contains("position")) {
            auto arr = j["position"];
            position_ = Vector3{arr[0], arr[1], arr[2]};
        }
        if (j.contains("rotation")) {
            auto arr = j["rotation"];
            rotation_ = Vector3{arr[0], arr[1], arr[2]};
        }
        if (j.contains("scale")) {
            auto arr = j["scale"];
            scale_ = Vector3{arr[0], arr[1], arr[2]};
        }
    }
}
