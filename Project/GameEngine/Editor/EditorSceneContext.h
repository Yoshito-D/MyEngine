#pragma once

#ifdef USE_IMGUI

#include "GameEngine/Editor/EditorAssetRegistry.h"
#include "GameEngine/Editor/EditorCommand.h"
#include "GameEngine/Scene/SceneObjectStore.h"
#include "GameEngine/Math/MathUtils.h"
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
   static constexpr int kCurrentSceneFormatVersion = 6;

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

   /// @brief シーン名とアセット一覧を初期化する
   /// @param sceneName 保存ファイル名の基準となるシーン名
   void Initialize(std::string sceneName);
   /// @brief 初回だけ保存済みエディタシーンを自動読込する
   void AutoLoad();
   /// @brief 選択・履歴・エディタ所有オブジェクトを破棄して初期状態へ戻す
   void Clear();

   /// @brief 現在の編集状態をシーンJSONへ保存する
   /// @return 保存に成功した場合はtrue
   bool Save();
   /// @brief シーンJSONを読み込み、現在の編集状態を置き換える
   /// @return 読み込みと復元に成功した場合はtrue
   bool Load();
   /// @brief シーン所有分とエディタ所有分を統合したJSONを構築する
   /// @return 保存可能なシーンJSON
   nlohmann::json SerializeToJson();
   /// @brief JSONからオブジェクト・パーティクル・カメラを復元する
   /// @param sceneData 読み込むシーンJSON
   /// @return 必要なシーン構造を適用できた場合はtrue
   /// @note 失敗時は保存前の状態と安定IDによる選択・履歴を復元する。旧リソースの復元失敗は状態メッセージで通知する。
   /// @note 描画コマンドが旧オブジェクトを参照していないフレーム境界で呼び出す
   bool LoadFromJson(const nlohmann::json& sceneData);
   /// @brief シーンJSONのヒエラルキー順を現在のオブジェクトへ適用する
   /// @param hierarchyOrderData オブジェクトIDの配列
   void ApplyHierarchyOrder(const nlohmann::json& hierarchyOrderData);
   /// @brief 現在のシーン名に対応する保存先を取得する
   /// @return resources配下のシーンJSONパス
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

   /// @brief 非表示指定を除くシーン・エディタ双方の編集可能オブジェクトを収集する
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
   ParticleSystem* GetSelectedParticleSystem() { return IsParticleSystemAlive(selectedParticleSystem_) ? selectedParticleSystem_ : nullptr; }
   /// @brief const Contextの選択状態を読み取り専用で参照する。
   const ParticleSystem* GetSelectedParticleSystem() const { return IsParticleSystemAlive(selectedParticleSystem_) ? selectedParticleSystem_ : nullptr; }

   /// @brief オブジェクトの親とヒエラルキー表示順を変更する
   /// @param movedObject 移動するオブジェクト
   /// @param targetObject ドロップ先。nullptrの場合はルート末尾へ移動
   /// @param dropPosition 対象の前・子・後ろのいずれへ配置するか
   /// @return 並び替えを適用できた場合はtrue
   bool ReorderObject(
      Object* movedObject,
      Object* targetObject,
      HierarchyDropPosition dropPosition);

   /// @brief オブジェクトがエディタの動的ストアに所有されているか調べる
   /// @param object 確認するオブジェクト
   /// @return エディタ所有の場合はtrue
   bool IsEditorOwned(const Object* object) const { return objectStore_.Contains(object); }
   /// @brief パーティクルがエディタの動的ストアに所有されているか調べる
   /// @param particleSystem 確認するパーティクル
   /// @return エディタ所有の場合はtrue
   bool IsEditorOwned(const ParticleSystem* particleSystem) const { return objectStore_.Contains(particleSystem); }
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
   const SceneObjectStore& GetObjectStore() const { return objectStore_; }
   /// @brief 安定IDを含む現在のObject状態を取得する。
   nlohmann::json GetObjectSnapshot(const Object* object);
   /// @brief 安定IDを含む現在のParticle状態を取得する。
   nlohmann::json GetParticleSnapshot(const ParticleSystem* particleSystem);
   /// @brief 適用済みInspector編集を一回のUndoとして確定する。
   /// @param id スナップショットに含まれる安定ID。
   /// @param before 操作開始時点の状態。
   /// @param after 操作終了時点の状態。
   /// @param name 履歴へ表示する操作名。
   bool CommitObjectEdit(const std::string& id, const nlohmann::json& before, const nlohmann::json& after, std::string name = "Edit Object");
   /// @brief 適用済みParticle編集を一回のUndoとして確定する。
   /// @copydetails CommitObjectEdit
   bool CommitParticleEdit(const std::string& id, const nlohmann::json& before, const nlohmann::json& after, std::string name = "Edit Particle System");
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

   /// @brief 現在のギズモ操作種別を取得する
   /// @return 平行移動・回転・拡縮のいずれか
   GizmoOperation GetGizmoOperation() const { return gizmoOperation_; }
   /// @brief ギズモ操作種別を設定する
   /// @param operation 平行移動・回転・拡縮のいずれか
   void SetGizmoOperation(GizmoOperation operation) { gizmoOperation_ = operation; }
   /// @brief 現在のギズモ座標空間を取得する
   /// @return ローカルまたはワールド
   GizmoMode GetGizmoMode() const { return gizmoMode_; }
   /// @brief ギズモ座標空間を設定する
   /// @param mode ローカルまたはワールド
   void SetGizmoMode(GizmoMode mode) { gizmoMode_ = mode; }

   /// @brief Transformインスペクター内へGuizmoの操作種別と座標空間を描画する
   void DrawGizmoInspectorControls();

   /// @brief 選択対象のトランスフォームギズモをビューポート上へ描画する
   /// @param viewportX ビューポート左上のX座標
   /// @param viewportY ビューポート左上のY座標
   /// @param viewportWidth ビューポート幅
   /// @param viewportHeight ビューポート高さ
   void DrawTransformGizmo(float viewportX, float viewportY, float viewportWidth, float viewportHeight);
   /// @brief ビューポートへの型検証済みモデル・パーティクルアセットドロップを受け付ける
   void AcceptViewportAssetDrop();
   /// @brief ビューポートクリック位置から描画IDを読み、対象を選択する
   /// @param viewportX ビューポート左上のX座標
   /// @param viewportY ビューポート左上のY座標
   /// @param viewportWidth ビューポート幅
   /// @param viewportHeight ビューポート高さ
   void HandleViewportClickSelection(float viewportX, float viewportY, float viewportWidth, float viewportHeight);

private:
   friend class EditorCommandStack;
   friend class CreateObjectCommand;
   friend class EditObjectStateCommand;
   friend class ReorderObjectCommand;
   friend class DeleteObjectCommand;
   friend class DeleteParticleSystemCommand;
   friend class TransformObjectCommand;
   friend class TransformParticleSystemCommand;
   friend class RestoreObjectSnapshotCommand;
   friend class ModifyComponentCommand;
   SceneObjectStore& CommandObjectStore() { return objectStore_; }
   bool IsObjectAlive(const Object* object) const;
   bool IsParticleSystemAlive(const ParticleSystem* particleSystem) const;
   void RegisterSceneOwnedKeys();
   std::string EnsureSceneObjectKey(const Object* object);
   std::string EnsureSceneParticleSystemKey(const ParticleSystem* particleSystem);
   Object* FindSceneObjectByKey(const std::string& key) const;
   ParticleSystem* FindSceneParticleSystemByKey(const std::string& key) const;
   nlohmann::json SerializeSceneObjects();
   nlohmann::json SerializeSceneParticleSystems();
   nlohmann::json SerializeCameras() const;
   bool ApplySceneSnapshot(const nlohmann::json& sceneData, bool& entitiesChanged);
   bool ApplySceneObjects(const nlohmann::json& sceneObjectsData);
   bool ApplySceneParticleSystems(const nlohmann::json& sceneParticlesData);
   void ApplyCameras(const nlohmann::json& camerasData);
   void HideSceneOwnedObject(Object* object);
   void HideSceneOwnedParticleSystem(ParticleSystem* particleSystem);
   bool HasTransformChanged(const Transform& lhs, const Transform& rhs) const;
   void SubmitTransformIfNeeded(const Transform& before, const Transform& after, Object* object);
   void SubmitParticleTransformIfNeeded(const Transform& before, const Transform& after, ParticleSystem* particleSystem);
   Transform BuildPlacementTransformInFrontOfCamera() const;
   std::string GetObjectIdForCommand(const Object* object) const;
   std::string GetParticleSystemIdForCommand(const ParticleSystem* particleSystem);
   ParticleSystem* FindParticleForCommand(const std::string& id) const;
   void SetStatus(std::string message);
   void ApplyDuplicateOffset(nlohmann::json& snapshot) const;

   std::string sceneName_ = "Scene";
   bool hasAutoLoaded_ = false;
   bool isDirty_ = false;
   bool referencesDirty_ = true;
   std::string lastStatusMessage_;
   Object* selectedObject_ = nullptr;
   ParticleSystem* selectedParticleSystem_ = nullptr;
   EditorAssetRegistry assetRegistry_;
   SceneObjectStore objectStore_;
   EditorCommandStack commandStack_;
   size_t editRevision_ = 0;
   std::unordered_set<const Object*> hiddenSceneObjects_;
   std::unordered_set<const ParticleSystem*> hiddenParticleSystems_;
   std::unordered_set<std::string> hiddenSceneObjectKeys_;
   std::unordered_set<std::string> hiddenParticleSystemKeys_;
   std::unordered_map<const Object*, std::string> sceneObjectKeys_;
   std::unordered_map<const ParticleSystem*, std::string> sceneParticleSystemKeys_;
   std::vector<std::string> hierarchyOrder_;

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
