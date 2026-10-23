#pragma once

#include <cstdint>
#ifdef USE_IMGUI

#include "GameEngine/Editor/EditorAssetRegistry.h"
#include "GameEngine/Editor/EditorCommand.h"
#include "GameEngine/Scene/SceneWorld.h"
#include "GameEngine/Scene/SceneDefinition.h"
#include "GameEngine/Math/MathUtils.h"
#include "GameEngine/Utility/Logger.h"
#include <cstddef>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace GameEngine {

class Object;
class ParticleSystem;

/// @brief エディタの選択・生成・保存・Undo履歴を1シーン単位で統括する
class EditorSceneContext {
public:
   /// @brief 新規保存するシーンJSONの現在の形式バージョン
   static constexpr std::int32_t kCurrentSceneFormatVersion = SceneWorld::kFormatVersion;

   /// @brief 編集するWorldを関連付け、所有権はシーンに残す。
   explicit EditorSceneContext(SceneWorld& world) : world_(&world) {}

   /// @brief ギズモで編集するトランスフォーム要素
   enum class GizmoOperation {
      Translate, ///< 平行移動
      Rotate,    ///< 回転
      Scale,     ///< 拡縮
   };

   /// @brief ギズモ軸の基準空間
   enum class GizmoMode {
      Local, ///< オブジェクト自身の回転に沿う軸
      World, ///< ワールド座標に固定された軸
   };

   /// @brief ヒエラルキーのドロップ位置
   enum class HierarchyDropPosition {
      Before, ///< 対象と同じ親の直前
      Into,   ///< 対象の末尾の子
      After,  ///< 対象と同じ親の直後
   };

   /// @brief シーン情報とアセット一覧を初期化する
   /// @param definition ロード・保存で共有するシーン名と実ファイルパス
   void Initialize(const SceneDefinition& definition);

   /// @brief シーンの実体を破棄する前に、選択中のポインターを切り離す。
   void Suspend();

   /// @brief 復元したWorldへ再接続し、必要に応じて編集履歴と未保存状態を引き継ぐ。
   void Rebind(SceneWorld& world, const SceneDefinition& definition, const nlohmann::json& data, bool preserve);

   /// @brief 選択と編集履歴を破棄して初期状態へ戻す
   void Clear();

   /// @brief 現在の編集状態をシーンJSONへ保存する
   /// @return 保存に成功した場合はtrue
   bool Save();

   /// @brief World全体の完全スナップショットを構築する
   /// @return 保存可能なシーンJSON
   nlohmann::json SerializeToJson();

   /// @brief シーンJSONのヒエラルキー順を現在のオブジェクトへ適用する
   /// @param hierarchyOrderData オブジェクトIDの配列
   void ApplyHierarchyOrder(const nlohmann::json& hierarchyOrderData);

   /// @brief 現在のシーン情報に登録された保存先を取得する
   /// @return ロード時に解決したシーンJSONの実ファイルパス
   std::filesystem::path GetSceneFilePath() const;

   /// @brief 未保存の編集があるか調べる
   /// @return 保存が必要な場合はtrue
   bool IsDirty() const { return isDirty_; }

   /// @brief 現在のシーンを保存が必要な状態にする
   void MarkDirty();

   /// @brief 編集で変更された参照を再接続する
   /// @param initializeRuntime trueなら未変更でも全Componentの実行時状態を初期化する
   void RefreshReferences(bool initializeRuntime = false);

   /// @brief 保存済み状態として変更フラグを解除する
   void ClearDirty();

   /// @brief 直近の保存・読込操作の状態メッセージを取得する
   /// @return エディタ表示用メッセージ
   const std::string& GetLastStatusMessage() const { return lastStatusMessage_; }

   /// @brief Worldが所有する編集可能オブジェクトを収集する
   /// @return 階層・選択UIへ表示できるオブジェクト一覧
   std::vector<Object*> CollectEditableObjects();

   /// @brief const Contextからは編集対象を読み取り専用で列挙する。
   std::vector<const Object*> CollectEditableObjects() const;

   /// @brief 非表示指定を除く編集可能パーティクルシステムを収集する
   /// @return 階層・選択UIへ表示できるパーティクル一覧
   std::vector<ParticleSystem*> CollectEditableParticleSystems();

   /// @brief const Contextからは編集対象を読み取り専用で列挙する。
   std::vector<const ParticleSystem*> CollectEditableParticleSystems() const;

   /// @brief 通常オブジェクトを単一選択し、パーティクル選択を解除する
   /// @param object 選択対象。nullptrで選択解除
   void SelectObject(Object* object);

   /// @brief 選択中の通常オブジェクトを取得する
   /// @return 選択対象。未選択の場合はnullptr
   Object* GetSelectedObject() { return IsObjectAlive(selectedObject_) ? selectedObject_ : nullptr; }

   /// @brief const Contextの選択状態を読み取り専用で参照する。
   const Object* GetSelectedObject() const { return IsObjectAlive(selectedObject_) ? selectedObject_ : nullptr; }

   /// @brief パーティクルを単一選択し、通常オブジェクト選択を解除する
   /// @param particleSystem 選択対象。nullptrで選択解除
   void SelectParticleSystem(ParticleSystem* particleSystem);

   /// @brief 選択中のパーティクルシステムを取得する
   /// @return 選択対象。未選択の場合はnullptr
   ParticleSystem* GetSelectedParticleSystem();

   /// @brief const Contextの選択状態を読み取り専用で参照する。
   const ParticleSystem* GetSelectedParticleSystem() const;

   /// @brief オブジェクトの親とヒエラルキー表示順を変更する
   /// @param movedObject 移動するオブジェクト
   /// @param targetObject ドロップ先。nullptrの場合はルート末尾へ移動
   /// @param dropPosition 対象の前・子・後ろのいずれへ配置するか
   /// @return 並び替えを適用できた場合はtrue
   bool ReorderObject(Object* movedObject, Object* targetObject, HierarchyDropPosition dropPosition);

   /// @brief 現在の選択対象を削除操作で扱えるか調べる
   /// @return 削除可能な選択がある場合はtrue
   bool CanDeleteSelectedObject() const;

   /// @brief 通常オブジェクトをエディタ上で削除または非表示にできるか調べる
   /// @param object 確認するオブジェクト
   /// @return 削除操作を適用できる場合はtrue
   bool CanDeleteObject(const Object* object) const;

   /// @brief パーティクルをエディタ上で削除または非表示にできるか調べる
   /// @param particleSystem 確認するパーティクル
   /// @return 削除操作を適用できる場合はtrue
   bool CanDeleteParticleSystem(const ParticleSystem* particleSystem) const;

   /// @brief ビューポート前方に描画コンポーネントを持たない空オブジェクトを作成する
   void CreateEmptyObject();

   /// @brief テクスチャを使うスプライトをビューポート前方へ配置する
   /// @param textureAssetId テクスチャのアセットID
   void CreateSpriteFromTexture(const std::string& textureAssetId);

   /// @brief ビューポート中央に編集可能なUIテキストを作成する
   void CreateUIText();

   /// @brief 編集可能なスカイボックスを作成する
   void CreateSkybox();

   /// @brief Directional Light Entityを作成する
   void CreateDirectionalLight();

   /// @brief Point Light Entityを作成する
   void CreatePointLight();

   /// @brief Spot Light Entityを作成する
   void CreateSpotLight();

   /// @brief Area Light Entityを作成する
   void CreateAreaLight();

   /// @brief 選択対象のスナップショットを複製し、重ならないよう位置をずらす
   void DuplicateSelectedObject();

   /// @brief 通常オブジェクトをUndo可能な形で削除または非表示にする
   /// @param object 削除対象
   void DeleteObject(Object* object);

   /// @brief パーティクルをUndo可能な形で削除または非表示にする
   /// @param particleSystem 削除対象
   void DeleteParticleSystem(ParticleSystem* particleSystem);

   /// @brief 現在選択中の通常オブジェクトを削除する
   void DeleteSelectedObject();

   /// @brief 種別にかかわらず現在の選択対象を削除する
   void DeleteSelection();

   /// @brief 選択中オブジェクトへ指定コンポーネントをUndo可能な形で追加する
   /// @param typeName ComponentRegistryへ登録済みの型名
   void AddComponentToSelectedObject(const std::string& typeName);

   /// @brief 選択中オブジェクトから指定コンポーネントをUndo可能な形で外す
   /// @param typeName 外すコンポーネント型名
   void RemoveComponentFromSelectedObject(const std::string& typeName);

   /// @brief 最新の編集コマンドを取り消す
   void Undo();

   /// @brief 最新の取り消し済み編集コマンドを再実行する
   void Redo();

   /// @brief 明示的なProject操作から再走査・ファイル操作を実行する。
   EditorAssetRegistry& GetAssetRegistry() { return assetRegistry_; }

   /// @brief スキャン済みアセット情報を読み取り専用で参照する。
   const EditorAssetRegistry& GetAssetRegistry() const { return assetRegistry_; }

   /// @brief 編集所有物とシーン実体のシリアライズ経路を参照する。
   const SceneObjectStore& GetObjectStore() const { return Store(); }

   /// @brief 安定IDを含む現在のObject状態を取得する。
   nlohmann::json GetObjectSnapshot(const Object* object);

   /// @brief 安定IDを含む現在のParticle状態を取得する。
   nlohmann::json GetParticleSnapshot(const ParticleSystem* particleSystem);

   /// @brief 適用済みInspector編集を一回のUndoとして確定する。
   /// @param id スナップショットに含まれる安定ID。
   /// @param before 操作開始時点の状態。
   /// @param after 操作終了時点の状態。
   /// @param name 履歴へ表示する操作名。
   bool CommitObjectEdit(const std::string& id, const nlohmann::json& before, const nlohmann::json& after,
      std::string name = "Edit Object");

   /// @brief 適用済みParticle編集を一回のUndoとして確定する。
   /// @copydetails CommitObjectEdit
   bool CommitParticleEdit(const std::string& id, const nlohmann::json& before, const nlohmann::json& after,
      std::string name = "Edit Particle System");

   /// @brief 入れ子の編集をInspectorから二重記録しないため、成功した履歴変更の番号を返す。
   size_t GetEditRevision() const { return editRevision_; }

   /// @brief Undo/Redo内に古いアセットIDを保持する履歴があるか調べる。
   bool HasEditHistory() const { return commandStack_.CanUndo() || commandStack_.CanRedo(); }

   /// @brief 正式IDのモデル・パーティクルを一操作として配置する。
   /// @param assetId スキャン済みリソース相対ID。
   /// @param parent モデルの親。nullptrでカメラ前方8単位のルート配置。Particleは親を持てない。
   /// @return 型・ロード・親検証が成功して配置された場合true。
   bool PlaceAsset(const std::string& assetId, Object* parent = nullptr);

   /// @brief 選択中のObjectまたはParticleの名前を一操作として変更する。
   /// @param name 空でない新しい名前。
   bool RenameSelectedObject(const std::string& name);

   /// @brief 編集コマンドを実行し、Undo履歴とdirty状態を一緒に記録する。
   bool CommitEdit(std::unique_ptr<IEditorCommand> command);

   /// @brief フレーム中の編集が完了した後に遅延削除を確定する。
   void FinishEditingFrame();

   /// @brief アセット候補を再走査する。
   void RefreshAssets();

private:
   friend class EditorSceneSession;

   class CommandOperations final : public EditorCommandTarget {
   public:
      /// @brief 実行中のコマンドにだけ所有者の編集操作を提供する。
      explicit CommandOperations(EditorSceneContext& owner) : owner_(owner) {}

      /// @copydoc EditorCommandTarget::Contains
      bool Contains(const std::string& id, bool particle) const override;

      /// @copydoc EditorCommandTarget::Capture
      nlohmann::json Capture(const std::string& id, bool particle) override;

      /// @copydoc EditorCommandTarget::Create
      nlohmann::json Create(CreateObjectCommand::Kind kind, const Transform& pose, const std::string& assetId,
         const std::string& parentId, const nlohmann::json& snapshot) override;

      /// @copydoc EditorCommandTarget::Remove
      bool Remove(const std::string& id, bool particle, RemovedEditorEntity& removed) override;

      /// @copydoc EditorCommandTarget::Restore
      bool Restore(const RemovedEditorEntity& removed, bool particle) override;

      /// @copydoc EditorCommandTarget::ApplySnapshot
      bool ApplySnapshot(const std::string& id, const nlohmann::json& snapshot, bool particle) override;

      /// @copydoc EditorCommandTarget::ApplyPose
      bool ApplyPose(const std::string& id, const Transform& pose, bool particle) override;

      /// @copydoc EditorCommandTarget::Reorder
      bool Reorder(const std::string& id, const std::string& parent, const std::vector<std::string>& order) override;

      /// @copydoc EditorCommandTarget::RestoreSnapshot
      nlohmann::json RestoreSnapshot(const nlohmann::json& snapshot) override;

      /// @copydoc EditorCommandTarget::ModifyComponent
      bool ModifyComponent(const std::string& id, const std::string& typeName, bool add) override;

   private:
      Object* ResolveObject(const std::string& id) const;
      EditorSceneContext& owner_; ///< この部品の接続先となる所有オブジェクトへの参照。
   };

   void FinishCommand(bool changed);
   bool IsObjectAlive(const Object* object) const;
   bool IsParticleSystemAlive(const ParticleSystem* particleSystem) const;
   Transform BuildPlacementTransformInFrontOfCamera() const;
   std::string GetObjectIdForCommand(const Object* object) const;
   std::string GetParticleSystemIdForCommand(const ParticleSystem* particleSystem);
   ParticleSystem* FindParticleForCommand(const std::string& id) const;
   void SetStatus(std::string message, Logger::LogLevel level = Logger::LogLevel::Info);
   void ApplyDuplicateOffset(nlohmann::json& snapshot) const;

   SceneDefinition definition_; ///< シーン名と保存先などのシーン定義。
   /// @brief suspendedObjectId_: シーン実体の差し替え後に選択を復元するObjectの安定ID。suspendedParticleId_:
   /// シーン実体の差し替え後に選択を復元するParticleSystemの安定ID。
   std::string suspendedObjectId_, suspendedParticleId_;
   bool isDirty_ = false;                             ///< 未保存の編集があるかどうか。
   bool referencesDirty_ = true;                      ///< 編集によってコンポーネント参照の再解決が必要かどうか。
   std::string lastStatusMessage_;                    ///< 直近の保存・読込操作の状態メッセージ。
   Object* selectedObject_ = nullptr;                 ///< 選択中の編集対象Objectへの非所有参照。
   ParticleSystem* selectedParticleSystem_ = nullptr; ///< 選択中のParticleSystemへの非所有参照。
   EditorAssetRegistry assetRegistry_;                ///< アセットの一覧・再走査・ファイル操作を管理するレジストリ。
   SceneWorld* world_;                                ///< 所属シーンのオブジェクトと実行時状態を参照するワールド。

   SceneObjectStore& Store() { return world_->GetObjectStore(); }

   const SceneObjectStore& Store() const { return world_->GetObjectStore(); }

   EditorCommandStack commandStack_; ///< 編集操作のUndo・Redo履歴。
   size_t editRevision_ = 0;         ///< 入れ子の編集をInspectorから二重記録しないため、成功した履歴変更の番号。
   std::vector<std::string> hierarchyOrder_; ///< ヒエラルキー表示順に並べた安定Entity ID。
};

} // namespace GameEngine

#endif
