#pragma once

#ifdef USE_IMGUI

#include "MathUtils.h"
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace GameEngine {

class EditorSceneContext;
class Object;

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

} // namespace GameEngine

#endif
