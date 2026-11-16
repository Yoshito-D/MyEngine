#include "GameEngine/pch.h"
#include "GameEngine/Graphics/Renderer/RenderQueue.h"
#include "GameEngine/Scene/Camera/Camera.h"
#include <algorithm>
#include <utility>

namespace GameEngine {
void RenderQueue::Clear() {
   for (auto& commands : commands_) {
      commands.clear();
   }
}

bool RenderQueue::Enqueue(DrawCommand command) {
   const auto index = static_cast<size_t>(command.GetRenderPass());

   if (index >= commands_.size()) {
      Logger::Warning("[RenderQueue] Draw command has an invalid render pass");
      return false;
   }

   commands_[index].push_back(std::move(command));
   return true;
}

std::span<const DrawCommand> RenderQueue::GetCommands(RenderPass renderPass) const {
   const auto index = static_cast<size_t>(renderPass);

   if (index >= commands_.size()) {
      return {};
   }

   return commands_[index];
}

void RenderQueue::SortTransparent() {
   auto& commands = commands_[static_cast<size_t>(RenderPass::Transparent)];

   if (commands.size() < 2) return;

   for (auto& command : commands) {
      command.RefreshSortData();
   }

   // 通常のアルファ合成のため奥から手前へ描く。同距離・同優先度の登録順は変えない。
   std::stable_sort(commands.begin(), commands.end(), [](const DrawCommand& lhs, const DrawCommand& rhs) {
      const auto& left = lhs.GetSortData();
      const auto& right = rhs.GetSortData();
      const bool leftHasPosition = left.position.has_value() && left.camera;
      const bool rightHasPosition = right.position.has_value() && right.camera;

      // 位置を持つ要素を距離ソート対象として先にまとめ、位置なし同士は種別優先度へ委ねる。
      if (leftHasPosition != rightHasPosition) {
         return leftHasPosition > rightHasPosition;
      }

      if (leftHasPosition && rightHasPosition) {
         const float leftDistance = (*left.position - left.camera->GetPosition()).LengthSquared();
         const float rightDistance = (*right.position - right.camera->GetPosition()).LengthSquared();

         if (leftDistance != rightDistance) {
            return leftDistance > rightDistance;
         }
      }

      return left.priority > right.priority;
   });
}
} // namespace GameEngine
