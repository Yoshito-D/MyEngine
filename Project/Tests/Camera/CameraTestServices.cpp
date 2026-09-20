// These test doubles isolate the production camera pipeline from the renderer and scene registry.
#include "Framework/EngineContext.h"
#include "Object/Object.h"
#include "Scene/Camera/Camera.h"
#include "Utility/Logger.h"
#include <stdexcept>

float GameEngine::EngineContext::GetDeltaTime() { return 1.0f / 60.0f; }
uint64_t GameEngine::EngineContext::GetGameFrameNumber() { return 0; }
void GameEngine::EngineContext::DrawUIText(std::string_view, const Vector2&, const TextStyle&) {}
void Logger::WriteLogEntry(const std::string&, LogLevel, LogChannel, std::source_location) {}
void Logger::WriteLogEntry(const std::wstring&, LogLevel, LogChannel, std::source_location) {}
GameEngine::Object* GameEngine::Object::FindByEntityId(const std::string&) { return nullptr; }
GameEngine::Matrix4x4 GameEngine::Object::GetWorldMatrix() const { return Matrix4x4::Identity(); }
void GameEngine::Camera::SetFovY(float) { throw std::logic_error("Unexpected GPU camera access"); }
void GameEngine::Camera::Update() { throw std::logic_error("Unexpected GPU camera access"); }
void GameEngine::Camera::SetCameraForGpuData() { throw std::logic_error("Unexpected GPU camera access"); }
GameEngine::Matrix4x4 GameEngine::Camera::GetProjectionMatrix() const { return Matrix4x4::Identity(); }
