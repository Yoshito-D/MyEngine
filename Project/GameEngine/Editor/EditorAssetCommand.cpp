#include "GameEngine/pch.h"
#include "GameEngine/Editor/EditorAssetCommand.h"

#ifdef USE_IMGUI

#include "GameEngine/Editor/EditorAssetRegistry.h"
#include <utility>
#include <nlohmann/json.hpp>

namespace GameEngine {
namespace {
bool IsEngineFolder(const std::string& id) {
   return id == "engine" || id.starts_with("engine/");
}
} // namespace

EditorAssetCommand::EditorAssetCommand(EditorAssetCommandRequest request) : request_(std::move(request)) {
}

const char* EditorAssetCommand::GetName() const {
   switch (request_.operation) {
      case EditorAssetOperation::CreateFolder:
         return "Create Folder";
      case EditorAssetOperation::Rename:
         return "Rename";
      case EditorAssetOperation::Move:
         return "Move";
      case EditorAssetOperation::Duplicate:
         return "Duplicate";
      case EditorAssetOperation::Delete:
         return "Delete";
      default:
         return "Unknown";
   }
}

std::string EditorAssetCommand::CheckAvailability(const EditorAssetCommandContext& context) const {
   if (!context.editMode) {
      return "Asset changes are only available in Edit mode.";
   }

   return CheckTarget(context);
}

std::string EditorAssetCommand::ValidateArguments(const EditorAssetCommandContext&) const {
   return {};
}

std::vector<std::string> EditorAssetCommand::GetReferences(const EditorAssetCommandContext&) const {
   return {};
}

bool EditorAssetCommand::HasActiveSceneReference(const EditorAssetCommandContext& context) const {
   // 保存されていないシーンも検証する。取得先が未接続ならパス変更を許可しない。
   if (!context.captureActiveScene) {
      return true;
   }

   const auto* asset = context.registry.FindAsset(request_.assetId);
   const std::string scene = context.captureActiveScene();
   return scene.find(nlohmann::json(request_.assetId).dump()) != std::string::npos ||
          (asset && scene.find(nlohmann::json(asset->displayName).dump()) != std::string::npos);
}

EditorAssetCommandResult EditorAssetCommand::Execute(EditorAssetCommandContext& context) const {
   EditorAssetCommandResult result;
   result.error = CheckAvailability(context);

   if (result.error.empty()) {
      result.error = ValidateArguments(context);
   }

   if (!result.error.empty()) {
      return result;
   }

   // ダイアログのキャッシュを信用せず、実行直前の検証結果だけでファイルを変更する。
   return ExecuteOperation(context);
}

CreateFolderAssetCommand::CreateFolderAssetCommand(EditorAssetCommandRequest request)
   : EditorAssetCommand(std::move(request)) {
}

std::string CreateFolderAssetCommand::CheckTarget(const EditorAssetCommandContext& context) const {
   if (IsEngineFolder(request_.assetId)) {
      return "Engine resource folders use built-in names and cannot be modified.";
   }

   if (!request_.assetId.empty() && !context.registry.FindAsset(request_.assetId, EditorAssetType::Folder)) {
      return "The parent folder no longer exists.";
   }

   return {};
}

std::string CreateFolderAssetCommand::ValidateArguments(const EditorAssetCommandContext&) const {
   return EditorAssetRegistry::IsValidName(request_.name)
             ? std::string{}
             : "Enter a valid filename; reserved names and path separators are not allowed.";
}

EditorAssetCommandResult CreateFolderAssetCommand::ExecuteOperation(EditorAssetCommandContext& context) const {
   EditorAssetCommandResult result;
   result.succeeded = context.registry.CreateFolder(request_.assetId, request_.name, result.assetId, result.error);
   return result;
}

DuplicateAssetCommand::DuplicateAssetCommand(EditorAssetCommandRequest request)
   : EditorAssetCommand(std::move(request)) {
}

std::string DuplicateAssetCommand::CheckTarget(const EditorAssetCommandContext& context) const {
   std::string reason;
   context.registry.CanDuplicate(request_.assetId, reason);
   return reason;
}

EditorAssetCommandResult DuplicateAssetCommand::ExecuteOperation(EditorAssetCommandContext& context) const {
   EditorAssetCommandResult result;
   result.succeeded = context.registry.Duplicate(request_.assetId, result.assetId, result.error);
   return result;
}

DeleteAssetCommand::DeleteAssetCommand(EditorAssetCommandRequest request) : EditorAssetCommand(std::move(request)) {
}

std::string DeleteAssetCommand::CheckTarget(const EditorAssetCommandContext& context) const {
   std::string reason;
   context.registry.CanRemove(request_.assetId, reason);
   return reason;
}

std::vector<std::string> DeleteAssetCommand::GetReferences(const EditorAssetCommandContext& context) const {
   auto references = context.registry.FindReferences(request_.assetId);

   if (HasActiveSceneReference(context)) {
      references.push_back("Active scene (including unsaved edits)");
   }

   return references;
}

EditorAssetCommandResult DeleteAssetCommand::ExecuteOperation(EditorAssetCommandContext& context) const {
   EditorAssetCommandResult result;
   result.succeeded = context.registry.Remove(request_.assetId, result.error);
   return result;
}

RenameOrMoveAssetCommand::RenameOrMoveAssetCommand(EditorAssetCommandRequest request)
   : EditorAssetCommand(std::move(request)) {
}

std::string RenameOrMoveAssetCommand::CheckTarget(const EditorAssetCommandContext& context) const {
   if (context.hasSceneEdits) {
      return "Save and reload the scene to release Undo references before changing path IDs.";
   }

   std::string reason;

   if (!context.registry.CanRenameOrMove(request_.assetId, reason)) {
      return reason;
   }

   if (HasActiveSceneReference(context)) {
      return "Referenced by the active scene; changing this path ID cannot update all references safely.";
   }

   return {};
}

std::string RenameOrMoveAssetCommand::GetDestinationId() const {
   if (request_.operation == EditorAssetOperation::Move) {
      return request_.destinationId;
   }

   const size_t separator = request_.assetId.find_last_of('/');
   return separator == std::string::npos ? request_.name : request_.assetId.substr(0, separator + 1) + request_.name;
}

std::string RenameOrMoveAssetCommand::ValidateArguments(const EditorAssetCommandContext& context) const {
   if (request_.operation == EditorAssetOperation::Rename && !EditorAssetRegistry::IsValidName(request_.name)) {
      return "Enter a valid filename; reserved names and path separators are not allowed.";
   }

   const std::string destination = GetDestinationId();

   if (destination.empty() || destination == request_.assetId) {
      return "The destination is unchanged or empty.";
   }

   const size_t separator = destination.find_last_of('/');
   const std::string folder = separator == std::string::npos ? "" : destination.substr(0, separator);

   if (IsEngineFolder(folder)) {
      return "Engine resource folders use built-in names and cannot be modified.";
   }

   if (!folder.empty() && !context.registry.FindAsset(folder, EditorAssetType::Folder)) {
      return "The destination folder does not exist.";
   }

   if (folder == request_.assetId || folder.starts_with(request_.assetId + "/")) {
      return "A folder cannot be moved inside itself.";
   }

   const auto* asset = context.registry.FindAsset(request_.assetId);
   const bool particleFolder = folder == "game/particles" || folder.starts_with("game/particles/") ||
                               folder == "particles" || folder.starts_with("particles/");

   if (asset && asset->type == EditorAssetType::Particle && !particleFolder) {
      return "Particle settings must stay inside the particles resource folder.";
   }

   return {};
}

EditorAssetCommandResult RenameOrMoveAssetCommand::ExecuteOperation(EditorAssetCommandContext& context) const {
   EditorAssetCommandResult result;
   const std::string destination = GetDestinationId();
   result.succeeded = context.registry.RenameOrMove(request_.assetId, destination, result.error);

   if (result.succeeded) {
      result.assetId = destination;
   }

   return result;
}

std::unique_ptr<EditorAssetCommand> CreateEditorAssetCommand(EditorAssetCommandRequest request) {
   switch (request.operation) {
      case EditorAssetOperation::CreateFolder:
         return std::make_unique<CreateFolderAssetCommand>(std::move(request));
      case EditorAssetOperation::Rename:
      case EditorAssetOperation::Move:
         return std::make_unique<RenameOrMoveAssetCommand>(std::move(request));
      case EditorAssetOperation::Duplicate:
         return std::make_unique<DuplicateAssetCommand>(std::move(request));
      case EditorAssetOperation::Delete:
         return std::make_unique<DeleteAssetCommand>(std::move(request));
      default:
         return nullptr;
   }
}
} // namespace GameEngine

#endif
