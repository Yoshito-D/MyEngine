#pragma once
#include "CameraKeyframe.h"
#include "CameraSequencer.h"
#include "OrbitalCameraController.h"
#include "Scene/Camera/Camera.h"
#include <string>
#include <vector>
#include <memory>

/// @brief カメラシーケンスをJSONから編集・保存するエディタークラス
class CameraSequenceEditor {
public:
   /// @brief シーケンスタイプ
   enum class SequenceType {
	  Keyframe,  // キーフレームベース
	  Orbital    // 軌道ベース
   };

   CameraSequenceEditor() = default;
   ~CameraSequenceEditor() = default;

   /// @brief JSONファイルから読み込み
   /// @param filePath ファイルパス
   /// @param groupName JSONグループ名（デフォルト: "CameraSequence"）
   /// @return 成功したらtrue
   bool LoadFromFile(const std::string& filePath, const std::string& groupName = "CameraSequence");

   /// @brief JSONファイルに保存
   /// @param filePath ファイルパス
   /// @param groupName JSONグループ名（デフォルト: "CameraSequence"）
   /// @return 成功したらtrue
   bool SaveToFile(const std::string& filePath, const std::string& groupName = "CameraSequence");

   /// @brief シーケンサーに適用
   /// @param sequencer 適用先のシーケンサー
   void ApplyToSequencer(CameraSequencer* sequencer);

   /// @brief 軌道カメラコントローラーに適用
   /// @param controller 適用先のコントローラー
   void ApplyToOrbitalController(OrbitalCameraController* controller);

   /// @brief シーケンサーからキャプチャ
   /// @param sequencer キャプチャ元のシーケンサー
   void CaptureFromSequencer(const CameraSequencer* sequencer);

   /// @brief 軌道カメラコントローラーからキャプチャ
   /// @param controller キャプチャ元のコントローラー
   void CaptureFromOrbitalController(const OrbitalCameraController* controller);

   /// @brief エディターウィンドウを表示（ImGui）
   /// @param camera プレビュー用のカメラ
   void ShowEditorWindow(GameEngine::Camera* camera);

   /// @brief キーフレームを追加
   /// @param keyframe 追加するキーフレーム
   void AddKeyframe(const CameraKeyframe& keyframe);

   /// @brief キーフレームを削除
   /// @param index 削除するインデックス
   void RemoveKeyframe(size_t index);

   /// @brief キーフレームを取得
   /// @param index 取得するインデックス
   /// @return キーフレームのポインタ（存在しない場合はnullptr）
   CameraKeyframe* GetKeyframe(size_t index);

   /// @brief シーケンスタイプを取得
   /// @return シーケンスタイプ
   SequenceType GetSequenceType() const { return sequenceType_; }

   /// @brief シーケンスタイプを設定
   /// @param type シーケンスタイプ
   void SetSequenceType(SequenceType type) { sequenceType_ = type; }

private:
   /// @brief キーフレームをJSONに変換
   void KeyframeToJson(const CameraKeyframe& keyframe, const std::string& groupName, size_t index);

   /// @brief JSONからキーフレームを読み込み
   CameraKeyframe JsonToKeyframe(const std::string& groupName, size_t index);

   /// @brief 軌道パラメータをJSONに変換
   void OrbitalParamsToJson(const OrbitalCameraController::OrbitParams& params, const std::string& groupName);

   /// @brief JSONから軌道パラメータを読み込み
   OrbitalCameraController::OrbitParams JsonToOrbitalParams(const std::string& groupName);

   /// @brief イージングタイプを文字列に変換
   static std::string EasingTypeToString(CameraKeyframe::EasingType type);

   /// @brief 文字列からイージングタイプに変換
   static CameraKeyframe::EasingType StringToEasingType(const std::string& str);

   /// @brief キーフレームエディターを表示
   void ShowKeyframeEditor(GameEngine::Camera* camera);

   /// @brief 軌道エディターを表示
   void ShowOrbitalEditor(GameEngine::Camera* camera);

   SequenceType sequenceType_ = SequenceType::Keyframe;
   std::string sequenceName_ = "CameraSequence";
   std::vector<CameraKeyframe> keyframes_;
   OrbitalCameraController::OrbitParams orbitalParams_;

   // プレビュー用
   std::unique_ptr<CameraSequencer> previewSequencer_ = nullptr;
   std::unique_ptr<OrbitalCameraController> previewOrbital_ = nullptr;
   bool showPreview_ = false;
};
