#include "pch.h"
#include "PlayModeController.h"

#include "EngineContext.h"
#include "SceneManager.h"
#include "BaseScene.h"

#ifdef USE_IMGUI
#include "Editor/EditorSceneContext.h"
#include "Object/Object.h"
#include "Component/TransformComponent.h"
#endif

#include <algorithm>
#include <fstream>

namespace GameEngine {

#ifdef USE_IMGUI
namespace {
constexpr int kSceneJsonIndentSize = 3;

bool PatchObjectComponentData(
   nlohmann::json& objectData,
   const std::string& fallbackObjectId,
   const std::string& objectId,
   const std::string& componentTypeName,
   const nlohmann::json& componentData,
   const std::string& parentEntityId) {
   if (!objectData.is_object()) {
      return false;
   }

   // 旧sceneObjectsではIDが埋め込みObjectではなくsceneKeyにしかないため、
   // 明示IDがない場合だけ呼び出し元のfallbackを使って照合する。
   std::string serializedObjectId = objectData.value("id", "");
   if (serializedObjectId.empty()) {
      serializedObjectId = fallbackObjectId;
   }
   if (serializedObjectId != objectId ||
      !objectData.contains("components") ||
      !objectData.at("components").is_array()) {
      return false;
   }

   // 対象Componentの型固有dataだけを差し替え、他Componentと有効状態はPlay開始時の値を保つ。
   for (auto& componentEntry : objectData["components"]) {
      if (!componentEntry.is_object() ||
         componentEntry.value("typeName", "") != componentTypeName) {
         continue;
      }

      // enabledはゲーム進行中に一時変更されるため、保存開始時の値を維持する。
      componentEntry["data"] = componentData;
      if (componentTypeName == TransformComponent::kTypeName) {
         // Object直下の親IDは復元時にTransformの値より後から適用されるため、両方を同期する。
         objectData["parentId"] = parentEntityId;
      }
      return true;
   }

   return false;
}

bool PatchSceneComponentData(
   nlohmann::json& sceneData,
   const std::string& objectId,
   const std::string& componentTypeName,
   const nlohmann::json& componentData,
   const std::string& parentEntityId) {
   if (!sceneData.is_object()) {
      return false;
   }

   // 現行objectsと旧sceneObjectsの両方を走査する。移行途中のファイルに同じEntityが
   // 両形式で存在しても、同じComponent値へ揃えることで保存結果を一貫させる。
   bool patched = false;
   if (sceneData.contains("objects") && sceneData.at("objects").is_array()) {
      for (auto& objectData : sceneData["objects"]) {
         patched = PatchObjectComponentData(
            objectData,
            {},
            objectId,
            componentTypeName,
            componentData,
            parentEntityId) || patched;
      }
   }

   if (sceneData.contains("sceneObjects") && sceneData.at("sceneObjects").is_array()) {
      for (auto& sceneObjectEntry : sceneData["sceneObjects"]) {
         if (!sceneObjectEntry.is_object() || sceneObjectEntry.value("deleted", false)) {
            continue;
         }

         const std::string sceneKey = sceneObjectEntry.value("sceneKey", "");
         // 旧版はobjectラッパー内、移行後はentry直下に本体があるため形を吸収する。
         nlohmann::json* objectData = &sceneObjectEntry;
         if (sceneObjectEntry.contains("object") && sceneObjectEntry.at("object").is_object()) {
            objectData = &sceneObjectEntry["object"];
         }
         patched = PatchObjectComponentData(
            *objectData,
            sceneKey,
            objectId,
            componentTypeName,
            componentData,
            parentEntityId) || patched;
      }
   }

   return patched;
}

bool LoadSceneJsonFile(const std::filesystem::path& filePath, nlohmann::json& sceneData) {
   std::ifstream file(filePath);
   if (!file.is_open()) {
      return false;
   }

   try {
      file >> sceneData;
   } catch (...) {
      return false;
   }
   return sceneData.is_object();
}

bool SaveSceneJsonFile(const std::filesystem::path& filePath, const nlohmann::json& sceneData) {
   // 書き込みを始める前に全JSONを文字列化し、例外で既存ファイルへ触れないようにする。
   std::string serializedSceneData;
   try {
      serializedSceneData = sceneData.dump(kSceneJsonIndentSize);
   } catch (...) {
      return false;
   }

   std::filesystem::path temporaryFilePath = filePath;
   temporaryFilePath += ".component-save.tmp";

   {
      std::ofstream file(temporaryFilePath, std::ios::trunc);
      if (!file.is_open()) {
         return false;
      }

      file << serializedSceneData;
      if (!file.good()) {
         file.close();
         std::error_code removeError;
         std::filesystem::remove(temporaryFilePath, removeError);
         return false;
      }
   }

   // 書き込み途中のJSONで既存シーンを壊さないよう、完了した一時ファイルだけを置換する。
   if (!MoveFileExW(
      temporaryFilePath.c_str(),
      filePath.c_str(),
      MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
      std::error_code removeError;
      std::filesystem::remove(temporaryFilePath, removeError);
      return false;
   }
   return true;
}
} // namespace
#endif

const char* ToString(PlayMode mode) {
   switch (mode) {
      case PlayMode::Playing:
         return "Playing";
      case PlayMode::Paused:
         return "Paused";
      case PlayMode::Edit:
      default:
         return "Edit";
   }
}

void PlayModeController::RequestPlay() {
   // UIイベント中に直接モードを変えず、フレーム境界のProcessRequestsで状態遷移を確定する。
   if (mode_ == PlayMode::Paused) {
      resumeRequested_ = true;
      return;
   }
   playRequested_ = true;
}

void PlayModeController::RequestStop() {
   stopRequested_ = true;
}

void PlayModeController::RequestPause() {
   pauseRequested_ = true;
}

void PlayModeController::RequestResume() {
   resumeRequested_ = true;
}

void PlayModeController::RequestStep() {
   stepRequested_ = true;
}

#ifdef USE_IMGUI
bool PlayModeController::SaveComponent(Object& object, const std::string& componentTypeName) {
   auto reportFailure = [](const std::string& reason) {
      Logger::Warning(
         "Play mode component save failed: " + reason,
         Logger::LogChannel::Editor);
      return false;
   };

   if (mode_ == PlayMode::Edit || !hasEditorSceneSnapshot_) {
      return reportFailure("play mode snapshot is not available");
   }
   if (componentTypeName.empty() || object.GetEntityId().empty()) {
      return reportFailure("component type or Entity ID is empty");
   }

   const IObjectComponent* component = object.GetComponentByTypeName(componentTypeName);
   if (!component) {
      return reportFailure("component is not attached to Entity " + object.GetEntityId());
   }

   // 停止時の復元元はコピー上で先に試し、Serialize/Patch失敗時に現在のSnapshotを汚さない。
   nlohmann::json componentData;
   nlohmann::json patchedSnapshot = editorSceneSnapshot_;
   try {
      componentData = component->Serialize();
      if (!PatchSceneComponentData(
         patchedSnapshot,
         object.GetEntityId(),
         componentTypeName,
         componentData,
         object.GetParentEntityId())) {
         return reportFailure("component was not present when play mode started");
      }
   } catch (...) {
      return reportFailure("component serialization failed");
   }

   BaseScene* scene = BaseScene::GetCurrentScene();
   EditorSceneContext* editorContext = scene ? scene->GetEditorSceneContext() : nullptr;
   if (!editorContext) {
      return reportFailure("editor scene context is not available");
   }

   // ディスクはPlay開始後に外部編集された可能性があるため、開始時Snapshotを丸ごと保存せず、
   // 最新ファイルへ対象Componentだけをパッチする。
   const std::filesystem::path sceneFilePath = editorContext->GetSceneFilePath();
   nlohmann::json savedSceneData;
   if (!LoadSceneJsonFile(sceneFilePath, savedSceneData)) {
      return reportFailure("could not load " + sceneFilePath.generic_string());
   }

   try {
      if (!PatchSceneComponentData(
         savedSceneData,
         object.GetEntityId(),
         componentTypeName,
         componentData,
         object.GetParentEntityId())) {
         return reportFailure("component was not found in " + sceneFilePath.generic_string());
      }
   } catch (...) {
      return reportFailure("saved scene data is invalid");
   }

   if (!SaveSceneJsonFile(sceneFilePath, savedSceneData)) {
      return reportFailure("could not write " + sceneFilePath.generic_string());
   }

   // ディスク保存が成功してから停止時の復元元を更新し、両者の不整合を避ける。
   editorSceneSnapshot_ = std::move(patchedSnapshot);
   Logger::Info(
      "Play mode component saved: " + object.GetEntityId() + "/" + componentTypeName,
      Logger::LogChannel::Editor);
   return true;
}
#endif

void PlayModeController::ProcessRequests(SceneManager& sceneManager) {
   // 毎フレーム既定値を「更新しない/時間0」に戻し、PlayingまたはStepが成立した場合だけ有効化する。
   shouldRunRuntimeUpdate_ = false;
   gameDeltaTime_ = 0.0f;

   // Stopは同フレームのPlay/Pause/Resumeより優先し、Snapshot復元後に別状態へ遷移しないようにする。
   if (stopRequested_) {
      StopPlaying(sceneManager);
      ClearTransitionRequests();
   } else {
      if (playRequested_) {
         if (mode_ == PlayMode::Edit) {
            StartPlaying(sceneManager);
         } else if (mode_ == PlayMode::Paused) {
            mode_ = PlayMode::Playing;
         }
      }

      if (pauseRequested_ && mode_ == PlayMode::Playing) {
         mode_ = PlayMode::Paused;
      }

      if (resumeRequested_ && mode_ == PlayMode::Paused) {
         mode_ = PlayMode::Playing;
      }
   }

   // Step要求はPaused時だけ一度消費する。Playing中の誤操作を後のPauseまで持ち越さない。
   const bool consumeStep = stepRequested_ && mode_ == PlayMode::Paused;
   ClearTransitionRequests();

   if (mode_ == PlayMode::Playing) {
      // 通常再生は実フレーム時間、Stepは再現性のある固定時間を使い、どちらもtimeScaleを最後に適用する。
      gameDeltaTime_ = EngineContext::GetUnscaledDeltaTime() * timeScale_;
      shouldRunRuntimeUpdate_ = true;
   } else if (consumeStep) {
      gameDeltaTime_ = stepDeltaTime_ * timeScale_;
      shouldRunRuntimeUpdate_ = true;
   }

   // EngineContext経由の全ゲーム処理へ、ここで確定した単一のDeltaTimeを配布する。
   EngineContext::SetGameDeltaTime(gameDeltaTime_);
}

void PlayModeController::SetTimeScale(float timeScale) {
   // 負の時間は各Componentが想定していないため0に制限し、一時停止相当として扱う。
   timeScale_ = std::max(0.0f, timeScale);
}

void PlayModeController::StopForSceneInitialization() {
   const bool wasInPlayMode = mode_ != PlayMode::Edit;
   mode_ = PlayMode::Edit;
   shouldRunRuntimeUpdate_ = false;
   gameDeltaTime_ = 0.0f;
   ClearTransitionRequests();
   EngineContext::SetGameDeltaTime(gameDeltaTime_);

   if (wasInPlayMode) {
	  // 遷移先のシーンを保持するため、通常のStopのような開始シーン復元は行わない。
	  ClearPlaySessionState();
   }
}

void PlayModeController::StartPlaying(SceneManager& sceneManager) {
   if (mode_ != PlayMode::Edit) {
      return;
   }

   // 再生中の変更を破棄できるよう、開始シーン名・完全Snapshot・Dirty状態を一組で保存する。
   playStartSceneName_ = sceneManager.GetCurrentSceneName();
   editorSceneSnapshot_ = nlohmann::json();
   hasEditorSceneSnapshot_ = false;
   playStartSceneWasDirty_ = false;

#ifdef USE_IMGUI
   if (BaseScene* scene = sceneManager.GetCurrentScene()) {
      if (EditorSceneContext* editorContext = scene->GetEditorSceneContext()) {
         editorSceneSnapshot_ = editorContext->SerializeToJson();
         hasEditorSceneSnapshot_ = editorSceneSnapshot_.is_object();
         playStartSceneWasDirty_ = editorContext->IsDirty();
      }
   }
#endif

   // Snapshot取得後にPlayingへ移し、Serialize中の処理からはまだEdit状態として見えるようにする。
   mode_ = PlayMode::Playing;
}

void PlayModeController::StopPlaying(SceneManager& sceneManager) {
   if (mode_ == PlayMode::Edit) {
      return;
   }

   // 復元処理がRuntime更新を誘発しないよう、シーンを戻す前にEditへ確定する。
   mode_ = PlayMode::Edit;

#ifdef USE_IMGUI
   // 再生中に別シーンへ遷移していても開始シーンを作り直し、その新しいEditorContextへSnapshotを適用する。
   if (!playStartSceneName_.empty() && hasEditorSceneSnapshot_) {
      if (sceneManager.ChangeScene(playStartSceneName_)) {
         if (BaseScene* scene = sceneManager.GetCurrentScene()) {
            if (EditorSceneContext* editorContext = scene->GetEditorSceneContext()) {
               if (editorContext->LoadFromJson(editorSceneSnapshot_)) {
                   // Snapshot内容だけでなく保存済み/未保存の表示状態もPlay開始前へ戻す。
                   if (playStartSceneWasDirty_) {
                     editorContext->MarkDirty();
                  } else {
                     editorContext->ClearDirty();
                  }
               }
            }
         }
      }
   }
#endif

   ClearPlaySessionState();
}

void PlayModeController::ClearTransitionRequests() {
   playRequested_ = false;
   stopRequested_ = false;
   pauseRequested_ = false;
   resumeRequested_ = false;
   stepRequested_ = false;
}

void PlayModeController::ClearPlaySessionState() {
   playStartSceneName_.clear();
   editorSceneSnapshot_ = nlohmann::json();
   hasEditorSceneSnapshot_ = false;
   playStartSceneWasDirty_ = false;
}

} // namespace GameEngine
