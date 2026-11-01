#include "GameProject/App/Component/Vehicle/VehicleLandingAligner.h"
#include "GameEngine/Editor/ParameterAssetEditor.h"
#include "GameEngine/Math/Functions/QuaternionOperations.h"
#include <algorithm>

#ifdef USE_IMGUI
#include "GameEngine/Editor/ImGui/ImGuiManager.h"
#endif

using namespace GameEngine;

namespace App {

// ---------------------------------------------------------------
// 公開設定
// ---------------------------------------------------------------

void VehicleLandingAligner::BeginAlign(const Quaternion& startRot, const Quaternion& targetRot) {
   // 補正の開始姿勢・目標姿勢・残り時間を設定する。
   // alignTime 秒かけて startRot → targetRot へ Slerp 補間する。
   // 既に補正中の場合も現在地から再スタートするため、スムーズに繋がる。
   alignStart_ = startRot;
   alignTarget_ = targetRot;
   alignTimer_ = alignTime;
}

bool VehicleLandingAligner::Evaluate(float deltaTime, Quaternion& rotation) {
   if (!IsEnabled() || alignTimer_ <= 0.0f) {
      return false;
   }

   // 移動の調整役から一度だけ進め、地上操舵より優先する姿勢候補を返す。
   alignTimer_ = std::max(0.0f, alignTimer_ - std::max(0.0f, deltaTime));
   rotation = CalcBlendedRotation();
   return true;
}

// ---------------------------------------------------------------
// 内部設定
// ---------------------------------------------------------------

Quaternion VehicleLandingAligner::CalcBlendedRotation() const {
   // タイマーが残り 0 なら目標姿勢をそのまま返す。
   if (alignTimer_ <= 0.0f) {
      return alignTarget_;
   }

   // t = 1 - (残り時間 / 総時間) で 0→1 に変化する補間パラメータを計算する。
   // 開始直後は t ≈ 0 (= startRot 側)、終了直前は t ≈ 1 (= targetRot 側)。
   // Slerp（球面線形補間）はクォータニオンの大円上を等角速度で補間するため、
   // 姿勢変化が一定速度で見える自然な補正になる。
   float t = 1.0f - std::clamp(alignTimer_ / alignTime, 0.0f, 1.0f);
   return Slerp(alignStart_, alignTarget_, t);
}

// ---------------------------------------------------------------
// ImGui / シリアライズ
// ---------------------------------------------------------------

#ifdef USE_IMGUI
void VehicleLandingAligner::DrawInspector() {
   auto Tr = GameEngine::LocalizeEditorText;
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);

   if (!GameEngine::DrawParameterInspectorHeader(header.c_str())) return;

   ImGui::Separator();
   ImGui::DragFloat(Tr("姿勢合わせ時間", "Align Time"), &alignTime, 0.01f, 0.05f, 2.0f);
   ImGui::Spacing();
   ImGui::Text("%s: %.2f  %s: %s", Tr("姿勢合わせタイマー", "Align Timer"), alignTimer_, Tr("姿勢合わせ中", "Aligning"),
      IsAligning() ? Tr("はい", "yes") : Tr("いいえ", "no"));
}
#endif

nlohmann::json VehicleLandingAligner::Serialize() const {
   nlohmann::json json;
   json["alignTime"] = alignTime;
   return json;
}

nlohmann::json VehicleLandingAligner::GetSharedParameters() const {
   return ExtractSharedParameters({});
}

void VehicleLandingAligner::Deserialize(const nlohmann::json& data) {
   ReadParameters(data, true);
}

void VehicleLandingAligner::ApplySharedParameters(const nlohmann::json& data) {
   ReadParameters(data, false);
}

void VehicleLandingAligner::ReadParameters(const nlohmann::json& data, [[maybe_unused]] bool initializeRuntime) {
   if (data.contains("alignTime")) {
      alignTime = data["alignTime"];
   }
}

} // namespace App
