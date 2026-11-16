#include "GameEngine/pch.h"
#include "GameEngine/Graphics/Renderer/DrawCommand.h"
#include "GameEngine/Object/Model/Model.h"
#include "GameEngine/Object/Component/Base/TransformComponent.h"
#include <utility>

namespace GameEngine {
namespace {
DrawSortData DescribeSortData(const ModelDrawData& data) {
   return {data.model ? std::optional<Vector3>(data.model->GetPosition()) : std::nullopt, data.camera,
      DrawSortPriority::Model};
}

DrawSortData DescribeSortData(const SpriteDrawData& data) {
   const auto* transform = data.sprite ? data.sprite->GetComponent<TransformComponent>() : nullptr;
   return {transform ? std::optional<Vector3>(transform->GetLocalPose().translation) : std::nullopt, data.camera,
      DrawSortPriority::SpriteAndText};
}

DrawSortData DescribeSortData(const UISpriteDrawData&) {
   // 画面空間UIにはカメラ距離を割り当てず、同じ優先度内の登録順を維持する。
   return {std::nullopt, nullptr, DrawSortPriority::SpriteAndText};
}

DrawSortData DescribeSortData(const TextDrawData&) {
   return {std::nullopt, nullptr, DrawSortPriority::SpriteAndText};
}

DrawSortData DescribeSortData(const ParticleDrawData& data) {
   return {std::nullopt, data.camera, DrawSortPriority::Particle};
}

DrawSortData DescribeSortData(const LineDrawData& data) {
   return {data.sortPosition, data.camera, DrawSortPriority::Line};
}
} // namespace

DrawCommand::DrawCommand(RenderPass renderPass, DrawPayload payload)
   : renderPass_(renderPass), payload_(std::move(payload)) {
   RefreshSortData();
}

void DrawCommand::RefreshSortData() {
   // 手動Draw後にTransformが変更された場合も、従来と同じソート直前の位置で比較する。
   sortData_ = std::visit([](const auto& data) { return DescribeSortData(data); }, payload_);
}
} // namespace GameEngine
