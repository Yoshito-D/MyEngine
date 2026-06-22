#pragma once

#ifdef USE_IMGUI

#include "EditorAssetRegistry.h"
#include "EditorCommand.h"
#include "EditorObjectStore.h"
#include "MathUtils.h"
#include <filesystem>
#include <string>
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

   std::vector<Object*> CollectEditableObjects() const;
   std::vector<ParticleSystem*> CollectEditableParticleSystems() const;
   void SelectObject(Object* object);
   Object* GetSelectedObject() const { return selectedObject_; }

   bool IsEditorOwned(const Object* object) const { return objectStore_.Contains(object); }
   bool CanDeleteSelectedObject() const;
   bool CanDeleteObject(const Object* object) const;
   bool CanDeleteParticleSystem(const ParticleSystem* particleSystem) const;

   void CreateModelFromAsset(const std::string& assetId);
   ParticleSystem* CreateParticleSystemFromAsset(const std::string& assetId);
   void DeleteObject(Object* object);
   void DeleteParticleSystem(ParticleSystem* particleSystem);
   void DeleteSelectedObject();
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
   void HideSceneOwnedObject(Object* object);
   void HideSceneOwnedParticleSystem(ParticleSystem* particleSystem);
   bool HasTransformChanged(const Transform& lhs, const Transform& rhs) const;
   void SubmitTransformIfNeeded(const Transform& before, const Transform& after, Object* object);
   std::string GetObjectIdForCommand(const Object* object) const;

   std::string sceneName_ = "Scene";
   bool hasAutoLoaded_ = false;
   Object* selectedObject_ = nullptr;
   EditorAssetRegistry assetRegistry_;
   EditorObjectStore objectStore_;
   EditorCommandStack commandStack_;
   std::unordered_set<const Object*> hiddenSceneObjects_;
   std::unordered_set<const ParticleSystem*> hiddenParticleSystems_;

   GizmoOperation gizmoOperation_ = GizmoOperation::Translate;
   GizmoMode gizmoMode_ = GizmoMode::Local;
   bool isManipulating_ = false;
   Object* manipulatingObject_ = nullptr;
   Transform transformBeforeManipulation_{};
};

} // namespace GameEngine

#endif
