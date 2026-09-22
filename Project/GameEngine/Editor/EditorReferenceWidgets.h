#pragma once
#ifdef USE_IMGUI
#include <string>

namespace GameEngine::EditorUI {
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
