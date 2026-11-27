#include "GameEngine/Editor/EditorPlaySession.h"

#ifdef USE_IMGUI
#include "GameEngine/Framework/EngineContext.h"
#include "GameEngine/Scene/SceneManager.h"
#include "GameEngine/Utility/JsonFile.h"
#include "GameEngine/Utility/Logger.h"
#include <cstdint>
#include <fstream>
#include <utility>

namespace GameEngine {
namespace {
constexpr std::int32_t kSessionFormatVersion = 1;
constexpr std::int32_t kSessionJsonIndentSize = 3;
}

EditorPlaySession::EditorPlaySession(SceneManager& scenes, Audio& audio, std::filesystem::path stateFile)
   : scenes_(scenes), stateFile_(std::move(stateFile)), playMode_(audio) {
   scenes_.SetLifecycleCallbacks(
      [this]() {
         session_.Detach();
         playMode_.StopForSceneInitialization();
      },
      [this](Scene& scene, const SceneSnapshot& snapshot) { session_.Attach(scene, snapshot.data, false); },
      [this](const Scene& scene) { return CaptureRecovery(scene); });
   EngineContext::SetPlayModeController(&playMode_);
   EngineContext::SetEditorSceneSession(&session_);
}

EditorPlaySession::~EditorPlaySession() {
   scenes_.SetLifecycleCallbacks({}, {}, {});
   session_.Detach();
   EngineContext::SetEditorSceneSession(nullptr);
   EngineContext::SetPlayModeController(nullptr);
}

SceneRecovery EditorPlaySession::CaptureRecovery(const Scene& scene) {
   auto* context = session_.GetContext();
   const bool fromPlay = playMode_.HasRecoverySnapshot();
   const bool wasDirty = fromPlay ? playMode_.GetPlayStartWasDirty() : context && context->IsDirty();
   auto snapshot = fromPlay ? playMode_.GetRecoverySnapshot()
                            : SceneSnapshot{scene.GetDefinition(),
                                 context ? context->SerializeToJson() : scene.GetSceneWorld().SerializeToJson()};

   return {std::move(snapshot), [this, fromPlay, wasDirty](Scene& restored, const SceneSnapshot& recovery) {
              RestoreEditorState(restored, recovery, fromPlay, wasDirty);
           }};
}

void EditorPlaySession::RestoreEditorState(Scene& scene, const SceneSnapshot& snapshot, bool fromPlay, bool wasDirty) {
   if (fromPlay) {
      session_.SetRestoreEditingStateOnAttach(true);
   }

   session_.Attach(scene, snapshot.data, !fromPlay);
   playMode_.StopAfterLoadFailure();
   if (auto* context = session_.GetContext()) {
      if (wasDirty) {
         context->MarkDirty();
      } else {
         context->ClearDirty();
      }
   }
}

std::string EditorPlaySession::ResolveInitialScene() {
   const auto& catalog = scenes_.GetCatalog();
   std::ifstream file(stateFile_);
   if (!file.is_open()) {
      if (std::filesystem::exists(stateFile_)) {
         Logger::Fail("Cannot open editor session state: " + stateFile_.generic_string(), Logger::LogChannel::Editor);
      }
      return catalog.GetInitialSceneName();
   }

   try {
      auto state = nlohmann::json::parse(file);
      const auto& version = state.at("version");
      if (!version.is_number_integer() || version != kSessionFormatVersion) {
         Logger::Fail(
            "Invalid editor session state version: " + stateFile_.generic_string(), Logger::LogChannel::Editor);
      }
      auto name = state.at("lastSceneName").get<std::string>();
      if (!catalog.Contains(name)) {
         Logger::Fail(
            "Unknown editor session scene: " + name + " in " + stateFile_.generic_string(), Logger::LogChannel::Editor);
      }
      return name;
   } catch (const nlohmann::json::exception& exception) {
      Logger::Fail("Invalid editor session state: " + stateFile_.generic_string() + ": " + exception.what(),
         Logger::LogChannel::Editor);
   }
}

void EditorPlaySession::SaveCurrentScene() const {
   const auto name = scenes_.GetCurrentSceneName();
   if (name.empty()) return;

   const nlohmann::json state = {{"version", kSessionFormatVersion}, {"lastSceneName", name}};
   if (!SaveJsonFileAtomically(stateFile_, state, kSessionJsonIndentSize)) {
      Logger::Warning(
         "Editor session state could not be saved: " + stateFile_.generic_string(), Logger::LogChannel::Editor);
   }
}

EditorPlaySession::FrameTiming EditorPlaySession::BeginFrame(float realSeconds) {
   playMode_.ProcessRequests(scenes_, realSeconds);
   return {playMode_.ShouldRunRuntimeUpdate(), !playMode_.IsPlaying(),
      playMode_.IsPaused() ? playMode_.GetStepDeltaTime() : realSeconds, playMode_.GetGameDeltaTime()};
}

void EditorPlaySession::Update() {
   session_.Update();
}

void EditorPlaySession::Draw() {
   session_.Draw();
}

} // namespace GameEngine
#endif
