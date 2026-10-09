#pragma once
#ifdef USE_IMGUI
#include "GameEngine/Editor/EditorAssetRegistry.h"
#include <functional>
#include <string>

namespace GameEngine::EditorUI {
/// @brief リソース種別に加えて用途別の制約を検証する任意の判定関数。
using AssetPredicate = std::function<bool(const EditorAssetEntry&)>;
/// @brief 共通アセット参照欄を描画し、候補選択・解除・検証済みドロップを受け付ける。
/// @param assetId resources基準の永続ID。選択・参照表示だけでは変更しない。
/// @param requiredType 許可する種別。Unknownは全登録アセットを許可する。
/// @param accepts Cubemapなど用途固有の追加検証。未指定なら種別だけを検証する。
/// @return IDが実際に変更された場合だけtrue。
bool AssetReference(const char* label, std::string& assetId, EditorAssetType requiredType,
   const AssetPredicate& accepts = {});
/// @brief ファイルパスを保存する既存の参照欄を共通ID選択へ接続する。毎描画のファイル走査は行わない。
/// @param resourcePath UTF-8のresources配下ファイルパス。選択時にだけ対応パスを更新する。
/// @param requiredType 受け付けるアセット種別。
/// @return 保存パスが実際に変更された場合だけtrue。
bool AssetFileReference(const char* label, std::string& resourcePath, EditorAssetType requiredType);
/// @brief 現在のDragDropTargetで共通ペイロードを検証し、配達されたアセットを返す。
/// @param requiredType 必要なアセット種別。Unknownは任意の登録済み種別を許可する。
/// @param accepts 用途固有の追加検証。無効な候補はハイライトも適用も行わない。
/// @return 現レジストリの項目。無効・プレビュー中・未配達の場合はnullptr。
/// @note 返されたポインターを再走査や次フレームを跨いで保持しない。
const EditorAssetEntry* AcceptAssetDrop(EditorAssetType requiredType = EditorAssetType::Unknown,
   const AssetPredicate& accepts = {});
/// @brief 参照欄からProjectで表示するよう要求された安定IDを取り出して要求を解除する。
/// @return 表示要求のID。要求がなければ空文字列。
std::string TakeAssetRevealRequest();
/// @brief シーン内Entityを名前で選択する。保存値は安定IDで、Hierarchyからのドロップも受け付ける
/// @param requiredComponent 空でなければ指定Componentを持つEntityだけを候補にする
/// @return 参照値が変更された場合はtrue
bool ObjectReference(const char* label, std::string& id, const char* requiredComponent = "");
/// @brief 仮想カメラを選択し、未解決・必要Component不足を表示する
/// @param requiredComponent 必要なカメラComponent名。空なら全カメラ
/// @return 参照値が変更された場合はtrue
bool CameraReference(const char* label, std::string& id, const char* requiredComponent = "");
/// @brief カタログに登録された遷移先シーンを選択する。空値で遷移を解除する
/// @return シーン名が変更された場合はtrue
bool SceneReference(const char* label, std::string& name);
/// @brief 編集内容の変更と参照の再解決要求を現在のエディタへ通知する
void MarkChanged();
}
#endif
