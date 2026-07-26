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

/// @brief エディタ操作を実行・取り消し可能な形で表すコマンドインターフェース
class IEditorCommand {
public:
   /// @brief 派生コマンドを基底ポインターから安全に破棄する
   virtual ~IEditorCommand() = default;
   /// @brief コマンドを実行またはRedoする
   /// @param context 操作対象のエディタシーン
   /// @return 操作を適用できた場合はtrue
   virtual bool Execute(EditorSceneContext& context) = 0;
   /// @brief 直前の実行結果を取り消す
   /// @param context 操作対象のエディタシーン
   virtual void Undo(EditorSceneContext& context) = 0;
   /// @brief Undo/Redoメニューへ表示する操作名を取得する
   /// @return コマンドが所有する有効な文字列
   virtual const char* GetName() const = 0;
};

/// @brief 実行済み・取り消し済みコマンドを所有してUndo/Redo履歴を管理する
class EditorCommandStack {
public:
   /// @brief コマンドを実行し、成功時だけUndo履歴へ積む
   /// @param command 実行後に履歴が所有するコマンド
   /// @param context 操作対象のエディタシーン
   /// @return コマンドを適用できた場合はtrue
   bool Execute(std::unique_ptr<IEditorCommand> command, EditorSceneContext& context);
   /// @brief 最新コマンドを取り消してRedo履歴へ移す
   /// @param context 操作対象のエディタシーン
   void Undo(EditorSceneContext& context);
   /// @brief 最新の取り消し済みコマンドを再実行する
   /// @param context 操作対象のエディタシーン
   void Redo(EditorSceneContext& context);
   /// @brief Undo/Redo両方の履歴を破棄する
   void Clear();

   /// @brief 取り消せるコマンドがあるか調べる
   /// @return Undo履歴が空でない場合はtrue
   bool CanUndo() const { return !undoStack_.empty(); }
   /// @brief 再実行できるコマンドがあるか調べる
   /// @return Redo履歴が空でない場合はtrue
   bool CanRedo() const { return !redoStack_.empty(); }
   /// @brief 次に取り消す操作名を取得する
   /// @return 操作名。履歴が空の場合は空文字列
   const char* GetUndoName() const;
   /// @brief 次に再実行する操作名を取得する
   /// @return 操作名。履歴が空の場合は空文字列
   const char* GetRedoName() const;

private:
   std::vector<std::unique_ptr<IEditorCommand>> undoStack_;
   std::vector<std::unique_ptr<IEditorCommand>> redoStack_;
};

/// @brief 描画コンポーネントを持たない汎用オブジェクトの作成をUndo/Redo可能にするコマンド
class CreateGenericObjectCommand final : public IEditorCommand {
public:
   /// @brief 空オブジェクト作成コマンドを構築する
   /// @param initialTransform 初期トランスフォーム
   explicit CreateGenericObjectCommand(Transform initialTransform = Transform());

   /// @copydoc IEditorCommand::Execute
   bool Execute(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::Undo
   void Undo(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::GetName
   const char* GetName() const override { return "Create Empty Object"; }

private:
   Transform initialTransform_{};
   std::string objectId_;
   nlohmann::json snapshot_;
};

/// @brief モデルアセットからオブジェクトを作成し、Undoで削除するコマンド
class CreateModelCommand final : public IEditorCommand {
public:
   /// @brief モデル作成コマンドを構築する
   /// @param assetId 読み込むモデルのアセットID
   /// @param initialTransform 初期トランスフォーム
   explicit CreateModelCommand(std::string assetId, Transform initialTransform = Transform());

   /// @copydoc IEditorCommand::Execute
   bool Execute(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::Undo
   void Undo(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::GetName
   const char* GetName() const override { return "Create Model"; }

private:
   std::string assetId_;
   Transform initialTransform_{};
   std::string objectId_;
   nlohmann::json snapshot_;
};

/// @brief テクスチャからスプライトを作成し、Undoで削除するコマンド
class CreateSpriteCommand final : public IEditorCommand {
public:
   /// @brief スプライト作成コマンドを構築する
   /// @param textureAssetId 使用するテクスチャのアセットID
   /// @param initialTransform 初期トランスフォーム
   explicit CreateSpriteCommand(std::string textureAssetId, Transform initialTransform = Transform());

   /// @copydoc IEditorCommand::Execute
   bool Execute(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::Undo
   void Undo(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::GetName
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

/// @brief JSONアセットからパーティクルシステムを作成し、Undoで削除するコマンド
class CreateParticleSystemCommand final : public IEditorCommand {
public:
   /// @brief パーティクル作成コマンドを構築する
   /// @param assetId 読み込むパーティクルのアセットID
   /// @param initialTransform 初期トランスフォーム
   explicit CreateParticleSystemCommand(std::string assetId, Transform initialTransform = Transform());

   /// @copydoc IEditorCommand::Execute
   bool Execute(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::Undo
   void Undo(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::GetName
   const char* GetName() const override { return "Create Particle System"; }

private:
   std::string assetId_;
   Transform initialTransform_{};
   std::string objectId_;
   nlohmann::json snapshot_;
};

/// @brief エディタ所有オブジェクトをスナップショット付きで削除するコマンド
class DeleteObjectCommand final : public IEditorCommand {
public:
   /// @brief オブジェクト削除コマンドを構築する
   /// @param objectId 削除対象のエディタオブジェクトID
   explicit DeleteObjectCommand(std::string objectId);

   /// @copydoc IEditorCommand::Execute
   bool Execute(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::Undo
   void Undo(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::GetName
   const char* GetName() const override { return "Delete Object"; }

private:
   std::string objectId_;
   nlohmann::json snapshot_;
};

/// @brief エディタ所有パーティクルをスナップショット付きで削除するコマンド
class DeleteParticleSystemCommand final : public IEditorCommand {
public:
   /// @brief パーティクル削除コマンドを構築する
   /// @param objectId 削除対象のエディタオブジェクトID
   explicit DeleteParticleSystemCommand(std::string objectId);

   /// @copydoc IEditorCommand::Execute
   bool Execute(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::Undo
   void Undo(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::GetName
   const char* GetName() const override { return "Delete Particle System"; }

private:
   std::string objectId_;
   nlohmann::json snapshot_;
};

/// @brief オブジェクトの変形前後を保持してギズモ操作をUndo可能にするコマンド
class TransformObjectCommand final : public IEditorCommand {
public:
   /// @brief オブジェクト変形コマンドを構築する
   /// @param objectId エディタ所有オブジェクトのID。シーン所有時は空文字
   /// @param fallbackObject IDで解決できないシーン所有オブジェクト
   /// @param before 操作前のトランスフォーム
   /// @param after 操作後のトランスフォーム
   TransformObjectCommand(std::string objectId, Object* fallbackObject, const Transform& before, const Transform& after);

   /// @copydoc IEditorCommand::Execute
   bool Execute(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::Undo
   void Undo(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::GetName
   const char* GetName() const override { return "Transform Object"; }

private:
   Object* ResolveObject(EditorSceneContext& context) const;
   void Apply(EditorSceneContext& context, const Transform& transform) const;

   std::string objectId_;
   Object* fallbackObject_ = nullptr;
   Transform before_{};
   Transform after_{};
};

/// @brief パーティクルシステムの変形前後を保持してギズモ操作をUndo可能にするコマンド
class TransformParticleSystemCommand final : public IEditorCommand {
public:
   /// @brief パーティクル変形コマンドを構築する
   /// @param objectId エディタ所有パーティクルのID。シーン所有時は空文字
   /// @param fallbackParticleSystem IDで解決できないシーン所有パーティクル
   /// @param before 操作前のトランスフォーム
   /// @param after 操作後のトランスフォーム
   TransformParticleSystemCommand(std::string objectId, ParticleSystem* fallbackParticleSystem, const Transform& before, const Transform& after);

   /// @copydoc IEditorCommand::Execute
   bool Execute(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::Undo
   void Undo(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::GetName
   const char* GetName() const override { return "Transform Particle System"; }

private:
   ParticleSystem* ResolveParticleSystem(EditorSceneContext& context) const;
   void Apply(EditorSceneContext& context, const Transform& transform) const;

   std::string objectId_;
   ParticleSystem* fallbackParticleSystem_ = nullptr;
   Transform before_{};
   Transform after_{};
};

/// @brief 任意オブジェクトのJSONスナップショットを復元する複製用コマンド
class RestoreObjectSnapshotCommand final : public IEditorCommand {
public:
   /// @brief スナップショット復元コマンドを構築する
   /// @param snapshot 復元するシリアライズ済みオブジェクト
   /// @param commandName Undo/Redoメニューへ表示する操作名
   RestoreObjectSnapshotCommand(nlohmann::json snapshot, std::string commandName);

   /// @copydoc IEditorCommand::Execute
   bool Execute(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::Undo
   void Undo(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::GetName
   const char* GetName() const override { return commandName_.c_str(); }

private:
   nlohmann::json snapshot_;
   std::string restoredObjectId_;
   std::string commandName_;
};

/// @brief モデルオブジェクトの参照アセット切り替えをUndo可能にするコマンド
class SetModelAssetCommand final : public IEditorCommand {
public:
   /// @brief モデルアセット変更コマンドを構築する
   /// @param objectId エディタ所有オブジェクトのID。シーン所有時は空文字
   /// @param fallbackObject IDで解決できないシーン所有オブジェクト
   /// @param beforeAssetId 変更前のモデルアセットID
   /// @param afterAssetId 変更後のモデルアセットID
   SetModelAssetCommand(std::string objectId, Object* fallbackObject, std::string beforeAssetId, std::string afterAssetId);

   /// @copydoc IEditorCommand::Execute
   bool Execute(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::Undo
   void Undo(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::GetName
   const char* GetName() const override { return "Set Model Asset"; }

private:
   Object* ResolveObject(EditorSceneContext& context) const;
   bool Apply(EditorSceneContext& context, const std::string& assetId) const;

   std::string objectId_;
   Object* fallbackObject_ = nullptr;
   std::string beforeAssetId_;
   std::string afterAssetId_;
};

/// @brief マテリアルスロットのテクスチャ切り替えをUndo可能にするコマンド
class SetMaterialTextureCommand final : public IEditorCommand {
public:
   /// @brief テクスチャ変更コマンドを構築する
   /// @param objectId エディタ所有オブジェクトのID。シーン所有時は空文字
   /// @param fallbackObject IDで解決できないシーン所有オブジェクト
   /// @param slot 変更するマテリアルスロット
   /// @param beforeTextureId 変更前のテクスチャアセットID
   /// @param afterTextureId 変更後のテクスチャアセットID
   SetMaterialTextureCommand(std::string objectId, Object* fallbackObject, size_t slot, std::string beforeTextureId, std::string afterTextureId);

   /// @copydoc IEditorCommand::Execute
   bool Execute(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::Undo
   void Undo(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::GetName
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

/// @brief オブジェクトへのコンポーネント追加をUndo可能にするコマンド
class AddComponentCommand final : public IEditorCommand {
public:
   /// @brief コンポーネント追加コマンドを構築する
   /// @param objectId エディタ所有オブジェクトのID。シーン所有時は空文字
   /// @param fallbackObject IDで解決できないシーン所有オブジェクト
   /// @param typeName 追加するコンポーネント型名
   AddComponentCommand(std::string objectId, Object* fallbackObject, std::string typeName);

   /// @copydoc IEditorCommand::Execute
   bool Execute(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::Undo
   void Undo(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::GetName
   const char* GetName() const override { return "Add Component"; }

private:
   Object* ResolveObject(EditorSceneContext& context) const;

   std::string objectId_;
   Object* fallbackObject_ = nullptr;
   std::string typeName_;
   nlohmann::json beforeSnapshot_;
};

/// @brief オブジェクトからコンポーネントを外し、Undoで設定ごと復元するコマンド
class RemoveComponentCommand final : public IEditorCommand {
public:
   /// @brief コンポーネント削除コマンドを構築する
   /// @param objectId エディタ所有オブジェクトのID。シーン所有時は空文字
   /// @param fallbackObject IDで解決できないシーン所有オブジェクト
   /// @param typeName 外すコンポーネント型名
   RemoveComponentCommand(std::string objectId, Object* fallbackObject, std::string typeName);

   /// @copydoc IEditorCommand::Execute
   bool Execute(EditorSceneContext& context) override;

   /// @copydoc IEditorCommand::Undo
   void Undo(EditorSceneContext& context) override;

   /// @copydoc IEditorCommand::GetName
   const char* GetName() const override { return "Remove Component"; }

private:
   Object* ResolveObject(EditorSceneContext& context) const;

   std::string objectId_;
   Object* fallbackObject_ = nullptr;
   std::string typeName_;
   nlohmann::json removedComponentData_;
};

} // namespace GameEngine

#endif
