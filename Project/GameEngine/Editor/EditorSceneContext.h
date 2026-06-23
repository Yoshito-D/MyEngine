#pragma once

#ifdef USE_IMGUI

#include "EditorAssetRegistry.h"
#include "EditorCommand.h"
#include "EditorObjectStore.h"
#include "MathUtils.h"
#include <cstddef>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace GameEngine {

class Object;
class ParticleSystem;

class EditorSceneContext {
public:
   enum class GizmoOperation {
      Translate,
      Rotate,
      Scale,
   };

   enum class GizmoMode {
      Local,
      World,
   };

   void Initialize(std::string sceneName);
   void AutoLoad();
   void Clear();

   bool Save();
   bool Load();
   std::filesystem::path GetSceneFilePath() const;
   bool IsDirty() const { return isDirty_; }
   void MarkDirty();
   void ClearDirty();
   const std::string& GetLastStatusMessage() const { return lastStatusMessage_; }

   std::vector<Object*> CollectEditableObjects() const;
   std::vector<ParticleSystem*> CollectEditableParticleSystems() const;
   void SelectObject(Object* object);
   Object* GetSelectedObject() const { return selectedObject_; }
   void SelectParticleSystem(ParticleSystem* particleSystem);
   ParticleSystem* GetSelectedParticleSystem() const { return selectedParticleSystem_; }

   bool IsEditorOwned(const Object* object) const { return objectStore_.Contains(object); }
   bool IsEditorOwned(const ParticleSystem* particleSystem) const { return objectStore_.Contains(particleSystem); }
   bool CanDeleteSelectedObject() const;
   bool CanDeleteObject(const Object* object) const;
   bool CanDeleteParticleSystem(const ParticleSystem* particleSystem) const;

   void CreateModelFromAsset(const std::string& assetId);
   void CreateSpriteFromTexture(const std::string& textureAssetId);
   ParticleSystem* CreateParticleSystemFromAsset(const std::string& assetId);
   void DuplicateSelectedObject();
   void DeleteObject(Object* object);
   void DeleteParticleSystem(ParticleSystem* particleSystem);
   void DeleteSelectedObject();
   void DeleteSelection();
   void AddComponentToSelectedObject(const std::string& typeName);
   void SetModelAsset(Object* object, const std::string& assetId);
   void SetMaterialTexture(Object* object, size_t slot, const std::string& textureAssetId);
   void Undo();
   void Redo();

   EditorAssetRegistry& GetAssetRegistry() { return assetRegistry_; }
   const EditorAssetRegistry& GetAssetRegistry() const { return assetRegistry_; }
   EditorObjectStore& GetObjectStore() { return objectStore_; }
   const EditorObjectStore& GetObjectStore() const { return objectStore_; }
   EditorCommandStack& GetCommandStack() { return commandStack_; }

   GizmoOperation GetGizmoOperation() const { return gizmoOperation_; }
   void SetGizmoOperation(GizmoOperation operation) { gizmoOperation_ = operation; }
   GizmoMode GetGizmoMode() const { return gizmoMode_; }
   void SetGizmoMode(GizmoMode mode) { gizmoMode_ = mode; }

   void DrawTransformGizmo(float viewportX, float viewportY, float viewportWidth, float viewportHeight);
   void AcceptModelAssetDrop();
   void HandleEditorShortcuts();
   void HandleViewportClickSelection(float viewportX, float viewportY, float viewportWidth, float viewportHeight);

private:
   bool IsObjectAlive(const Object* object) const;
   bool IsParticleSystemAlive(const ParticleSystem* particleSystem) const;
   void RegisterSceneOwnedKeys();
   std::string EnsureSceneObjectKey(const Object* object);
   std::string EnsureSceneParticleSystemKey(const ParticleSystem* particleSystem);
   Object* FindSceneObjectByKey(const std::string& key) const;
   ParticleSystem* FindSceneParticleSystemByKey(const std::string& key) const;
   nlohmann::json SerializeSceneObjects();
   nlohmann::json SerializeSceneParticleSystems();
   void ApplySceneObjects(const nlohmann::json& sceneObjectsData);
   void ApplySceneParticleSystems(const nlohmann::json& sceneParticlesData);
   void HideSceneOwnedObject(Object* object);
   void HideSceneOwnedParticleSystem(ParticleSystem* particleSystem);
   bool HasTransformChanged(const Transform& lhs, const Transform& rhs) const;
   void SubmitTransformIfNeeded(const Transform& before, const Transform& after, Object* object);
   void SubmitParticleTransformIfNeeded(const Transform& before, const Transform& after, ParticleSystem* particleSystem);
   Transform BuildPlacementTransformInFrontOfCamera() const;
   std::string GetObjectIdForCommand(const Object* object) const;
   std::string GetParticleSystemIdForCommand(const ParticleSystem* particleSystem) const;
   void SetStatus(std::string message);
   void ApplyDuplicateOffset(nlohmann::json& snapshot) const;

   std::string sceneName_ = "Scene";
   bool hasAutoLoaded_ = false;
   bool isDirty_ = false;
   std::string lastStatusMessage_;
   Object* selectedObject_ = nullptr;
   ParticleSystem* selectedParticleSystem_ = nullptr;
   EditorAssetRegistry assetRegistry_;
   EditorObjectStore objectStore_;
   EditorCommandStack commandStack_;
   std::unordered_set<const Object*> hiddenSceneObjects_;
   std::unordered_set<const ParticleSystem*> hiddenParticleSystems_;
   std::unordered_set<std::string> hiddenSceneObjectKeys_;
   std::unordered_set<std::string> hiddenParticleSystemKeys_;
   std::unordered_map<const Object*, std::string> sceneObjectKeys_;
   std::unordered_map<const ParticleSystem*, std::string> sceneParticleSystemKeys_;

   GizmoOperation gizmoOperation_ = GizmoOperation::Translate;
   GizmoMode gizmoMode_ = GizmoMode::Local;
   bool isManipulating_ = false;
   Object* manipulatingObject_ = nullptr;
   Transform transformBeforeManipulation_{};
   bool isManipulatingParticleSystem_ = false;
   ParticleSystem* manipulatingParticleSystem_ = nullptr;
   Transform particleTransformBeforeManipulation_{};
};

} // namespace GameEngine

#endif
