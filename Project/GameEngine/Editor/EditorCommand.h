#pragma once

#ifdef USE_IMGUI

#include "MathUtils.h"
#include <cstddef>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace GameEngine {

class EditorSceneContext;
class Object;
class ParticleSystem;

class IEditorCommand {
public:
   virtual ~IEditorCommand() = default;
   virtual bool Execute(EditorSceneContext& context) = 0;
   virtual void Undo(EditorSceneContext& context) = 0;
   virtual const char* GetName() const = 0;
};

class EditorCommandStack {
public:
   bool Execute(std::unique_ptr<IEditorCommand> command, EditorSceneContext& context);
   void Undo(EditorSceneContext& context);
   void Redo(EditorSceneContext& context);
   void Clear();

   bool CanUndo() const { return !undoStack_.empty(); }
   bool CanRedo() const { return !redoStack_.empty(); }
   const char* GetUndoName() const;
   const char* GetRedoName() const;

private:
   std::vector<std::unique_ptr<IEditorCommand>> undoStack_;
   std::vector<std::unique_ptr<IEditorCommand>> redoStack_;
};

class CreateModelCommand final : public IEditorCommand {
public:
   explicit CreateModelCommand(std::string assetId, Transform initialTransform = Transform());

   bool Execute(EditorSceneContext& context) override;
   void Undo(EditorSceneContext& context) override;
   const char* GetName() const override { return "Create Model"; }

private:
   std::string assetId_;
   Transform initialTransform_{};
   std::string objectId_;
   nlohmann::json snapshot_;
};

class CreateSpriteCommand final : public IEditorCommand {
public:
   explicit CreateSpriteCommand(std::string textureAssetId, Transform initialTransform = Transform());

   bool Execute(EditorSceneContext& context) override;
   void Undo(EditorSceneContext& context) override;
   const char* GetName() const override { return "Create Sprite"; }

private:
   std::string textureAssetId_;
   Transform initialTransform_{};
   std::string objectId_;
   nlohmann::json snapshot_;
};

/// @brief UIテキストの作成をUndo/Redo可能にするコマンド
class CreateUITextCommand final : public IEditorCommand {
public:
   /// @brief UIテキスト作成コマンドを構築する
   /// @param initialTransform 初期スクリーン座標
   explicit CreateUITextCommand(Transform initialTransform = Transform());

   /// @copydoc IEditorCommand::Execute
   bool Execute(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::Undo
   void Undo(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::GetName
   const char* GetName() const override { return "Create UI Text"; }

private:
   Transform initialTransform_{};
   std::string objectId_;
   nlohmann::json snapshot_;
};

class CreateParticleSystemCommand final : public IEditorCommand {
public:
   explicit CreateParticleSystemCommand(std::string assetId, Transform initialTransform = Transform());

   bool Execute(EditorSceneContext& context) override;
   void Undo(EditorSceneContext& context) override;
   const char* GetName() const override { return "Create Particle System"; }

private:
   std::string assetId_;
   Transform initialTransform_{};
   std::string objectId_;
   nlohmann::json snapshot_;
};

class DeleteObjectCommand final : public IEditorCommand {
public:
   explicit DeleteObjectCommand(std::string objectId);

   bool Execute(EditorSceneContext& context) override;
   void Undo(EditorSceneContext& context) override;
   const char* GetName() const override { return "Delete Object"; }

private:
   std::string objectId_;
   nlohmann::json snapshot_;
};

class DeleteParticleSystemCommand final : public IEditorCommand {
public:
   explicit DeleteParticleSystemCommand(std::string objectId);

   bool Execute(EditorSceneContext& context) override;
   void Undo(EditorSceneContext& context) override;
   const char* GetName() const override { return "Delete Particle System"; }

private:
   std::string objectId_;
   nlohmann::json snapshot_;
};

class TransformObjectCommand final : public IEditorCommand {
public:
   TransformObjectCommand(std::string objectId, Object* fallbackObject, const Transform& before, const Transform& after);

   bool Execute(EditorSceneContext& context) override;
   void Undo(EditorSceneContext& context) override;
   const char* GetName() const override { return "Transform Object"; }

private:
   Object* ResolveObject(EditorSceneContext& context) const;
   void Apply(EditorSceneContext& context, const Transform& transform) const;

   std::string objectId_;
   Object* fallbackObject_ = nullptr;
   Transform before_{};
   Transform after_{};
};

class TransformParticleSystemCommand final : public IEditorCommand {
public:
   TransformParticleSystemCommand(std::string objectId, ParticleSystem* fallbackParticleSystem, const Transform& before, const Transform& after);

   bool Execute(EditorSceneContext& context) override;
   void Undo(EditorSceneContext& context) override;
   const char* GetName() const override { return "Transform Particle System"; }

private:
   ParticleSystem* ResolveParticleSystem(EditorSceneContext& context) const;
   void Apply(EditorSceneContext& context, const Transform& transform) const;

   std::string objectId_;
   ParticleSystem* fallbackParticleSystem_ = nullptr;
   Transform before_{};
   Transform after_{};
};

class RestoreObjectSnapshotCommand final : public IEditorCommand {
public:
   RestoreObjectSnapshotCommand(nlohmann::json snapshot, std::string commandName);

   bool Execute(EditorSceneContext& context) override;
   void Undo(EditorSceneContext& context) override;
   const char* GetName() const override { return commandName_.c_str(); }

private:
   nlohmann::json snapshot_;
   std::string restoredObjectId_;
   std::string commandName_;
};

class SetModelAssetCommand final : public IEditorCommand {
public:
   SetModelAssetCommand(std::string objectId, Object* fallbackObject, std::string beforeAssetId, std::string afterAssetId);

   bool Execute(EditorSceneContext& context) override;
   void Undo(EditorSceneContext& context) override;
   const char* GetName() const override { return "Set Model Asset"; }

private:
   Object* ResolveObject(EditorSceneContext& context) const;
   bool Apply(EditorSceneContext& context, const std::string& assetId) const;

   std::string objectId_;
   Object* fallbackObject_ = nullptr;
   std::string beforeAssetId_;
   std::string afterAssetId_;
};

class SetMaterialTextureCommand final : public IEditorCommand {
public:
   SetMaterialTextureCommand(std::string objectId, Object* fallbackObject, size_t slot, std::string beforeTextureId, std::string afterTextureId);

   bool Execute(EditorSceneContext& context) override;
   void Undo(EditorSceneContext& context) override;
   const char* GetName() const override { return "Set Texture"; }

private:
   Object* ResolveObject(EditorSceneContext& context) const;
   bool Apply(EditorSceneContext& context, const std::string& textureId) const;

   std::string objectId_;
   Object* fallbackObject_ = nullptr;
   size_t slot_ = 0;
   std::string beforeTextureId_;
   std::string afterTextureId_;
};

class AddComponentCommand final : public IEditorCommand {
public:
   AddComponentCommand(std::string objectId, Object* fallbackObject, std::string typeName);

   bool Execute(EditorSceneContext& context) override;
   void Undo(EditorSceneContext& context) override;
   const char* GetName() const override { return "Add Component"; }

private:
   Object* ResolveObject(EditorSceneContext& context) const;

   std::string objectId_;
   Object* fallbackObject_ = nullptr;
   std::string typeName_;
   nlohmann::json beforeSnapshot_;
};

} // namespace GameEngine

#endif
