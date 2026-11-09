#include "GameEngine/pch.h"
#include "GameEngine/UI/UILayoutSolver.h"
#include "GameEngine/UI/UIComponents.h"
#include "GameEngine/UI/UIAnimationComponent.h"
#include "GameEngine/Object/Object.h"
#include "GameEngine/Object/Component/Base/TransformComponent.h"
#include "GameEngine/Object/Component/Rendering/RenderComponent.h"
#include "GameEngine/Scene/SceneWorld.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <unordered_map>

namespace GameEngine {
namespace {
constexpr float kInverseTolerance = 0.000001f;

std::vector<const Object*> OrderedObjects(const SceneWorld& world, const std::vector<std::string>& hierarchyOrder) {
   auto objects = world.GetObjectStore().GetObjects();
   std::unordered_map<std::string, size_t> ranks;

   for (size_t index = 0; index < hierarchyOrder.size(); ++index) {
      ranks.try_emplace(hierarchyOrder[index], index);
   }

   std::stable_sort(objects.begin(), objects.end(), [&](const Object* a, const Object* b) {
      const auto left = ranks.find(a->GetEntityId());
      const auto right = ranks.find(b->GetEntityId());

      if ((left != ranks.end()) != (right != ranks.end())) {
         return left != ranks.end();
      }

      return left != ranks.end() && left->second < right->second;
   });
   return objects;
}

}

std::vector<UIElementLayout> UILayoutSolver::Build(const SceneWorld& world,
   const std::vector<std::string>& hierarchyOrder, const std::vector<std::string>& screenStack, Vector2 viewportSize,
   bool editing) {
   std::vector<UIElementLayout> elements;
   std::vector<std::string> stack;

   for (const auto& id : screenStack) {
      const auto* root = world.FindObjectById(id);
      const auto* screen = root ? root->GetComponent<UIScreenComponent>() : nullptr;
      const auto* canvas = root ? root->GetComponent<UICanvasComponent>() : nullptr;

      if (screen && canvas && screen->IsEnabled() && canvas->IsEnabled()) {
         stack.push_back(id);
      }
   }

   if (viewportSize.x <= 0.0f || viewportSize.y <= 0.0f) {
      return elements;
   }

   const auto objects = OrderedObjects(world, hierarchyOrder);
   std::unordered_map<std::string, std::vector<const Object*>> children;

   for (auto* object : objects) {
      children[object->GetParentEntityId()].push_back(object);
   }

   std::int32_t drawFloor = 0;
   std::int32_t inputFloor = 0;

   for (size_t i = 0; i < stack.size(); ++i) {
      const auto& settings = world.FindObjectById(stack[i])->GetComponent<UIScreenComponent>()->DescribeSettings();

      if (!settings.drawBelow) {
         drawFloor = static_cast<std::int32_t>(i);
      }

      if (settings.blocksBelowInput) {
         inputFloor = static_cast<std::int32_t>(i);
      }
   }

   size_t hierarchyIndex = 0;
   std::function<void(const Object*, const UIElementLayout&, Vector2, bool)> visit;
   visit = [&](const Object* object, const UIElementLayout& parent, Vector2 automaticPosition, bool automatic) {
      UIElementLayout item = parent;
      item.objectId = object->GetEntityId();
      item.hierarchyOrder = hierarchyIndex++;
      UIRectTransformSettings rect;

      if (const auto* component = object->GetComponent<UIRectTransformComponent>();
         component && component->IsEnabled()) {
         rect = component->DescribeSettings();
      }

      item.size = {rect.size[0], rect.size[1]};
      item.sortingOrder = rect.sortingOrder;

      if (const auto* canvas = object->GetComponent<UICanvasComponent>()) {
         const auto& size = canvas->DescribeSettings().referenceSize;
         item.size = {size[0], size[1]};
      }

      Transform pose;

      if (const auto* transform = object->GetComponent<TransformComponent>()) {
         pose = transform->GetLocalPose();
      }

      pose.translation.x += automatic ? automaticPosition.x + rect.position[0] + rect.pivot[0] * item.size.x
                                      : parent.size.x * rect.anchor[0] + rect.position[0];
      pose.translation.y += automatic ? automaticPosition.y + rect.position[1] + rect.pivot[1] * item.size.y
                                      : parent.size.y * rect.anchor[1] + rect.position[1];
      Transform pivot;
      pivot.translation = {-rect.pivot[0] * item.size.x, -rect.pivot[1] * item.size.y, 0.0f};
      item.matrix = MakeAffineMatrix(pivot) * MakeAffineMatrix(pose) * parent.matrix;

      if (const auto* animation = object->GetComponent<UIAnimationComponent>(); animation && animation->IsEnabled()) {
         const auto animated = animation->GetPose(editing);
         Transform center;
         center.translation = {-item.size.x * 0.5f, -item.size.y * 0.5f, 0.0f};
         Transform effect;
         effect.scale = {animated.scale.x, animated.scale.y, 1.0f};
         effect.SetRotationEuler({0.0f, 0.0f, animated.rotation});
         effect.translation = {item.size.x * 0.5f + animated.offset.x, item.size.y * 0.5f + animated.offset.y, 0.0f};
         item.matrix = MakeAffineMatrix(center) * MakeAffineMatrix(effect) * item.matrix;
         item.opacity *= animated.opacity;
      }

      if (const auto* group = object->GetComponent<UIGroupComponent>(); group && group->IsEnabled()) {
         item.visible = item.visible && group->DescribeSettings().visible;
         item.interactable = item.interactable && group->DescribeSettings().interactable;
         item.opacity *= group->DescribeSettings().opacity;
      }

      if (const auto* render = object->GetComponent<RenderComponent>()) {
         item.visible = item.visible && render->IsEnabled() && render->DescribeSettings().visible;
      }

      if (const auto* widget = GetUIWidget(*object)) {
         item.interactable = item.interactable && widget->IsEnabled() && widget->DescribeSettings().interactable;
      }

      elements.push_back(item);
      const auto* layout = object->GetComponent<UILayoutGroupComponent>();
      const bool align = layout && layout->IsEnabled();
      Vector2 cursor{};

      if (align) {
         cursor = {layout->DescribeSettings().padding[0], layout->DescribeSettings().padding[1]};
      }

      for (auto* child : children[object->GetEntityId()]) {
         if (child->GetComponent<UICanvasComponent>()) {
            continue;
         }

         visit(child, item, cursor, align);

         if (align) {
            const auto* childRect = child->GetComponent<UIRectTransformComponent>();
            const auto size = childRect ? childRect->DescribeSettings().size : UIRectTransformSettings{}.size;

            if (layout->DescribeSettings().horizontal) {
               cursor.x += size[0] + layout->DescribeSettings().spacing;
            } else {
               cursor.y += size[1] + layout->DescribeSettings().spacing;
            }
         }
      }
   };

   for (auto* root : objects) {
      const auto* canvas = root->GetComponent<UICanvasComponent>();

      if (!canvas || !canvas->IsEnabled()) {
         continue;
      }

      const auto it = std::find(stack.begin(), stack.end(), root->GetEntityId());
      const auto order = static_cast<std::int32_t>(std::distance(stack.begin(), it));
      UIElementLayout parent;
      parent.screenId = root->GetEntityId();
      parent.screenOrder = order;
      parent.visible = editing || (it != stack.end() && order >= drawFloor);
      parent.interactable = !editing && it != stack.end() && order >= inputFloor;
      const auto& size = canvas->DescribeSettings().referenceSize;
      const float scale = std::min(viewportSize.x / size[0], viewportSize.y / size[1]);
      Transform canvasPose;
      canvasPose.scale = {scale, scale, 1.0f};
      canvasPose.translation = {
         (viewportSize.x - size[0] * scale) * 0.5f, (viewportSize.y - size[1] * scale) * 0.5f, 0.0f};
      parent.matrix = MakeAffineMatrix(canvasPose);
      visit(root, parent, {}, false);
   }

   std::stable_sort(elements.begin(), elements.end(), [](const auto& a, const auto& b) {
      if (a.screenOrder != b.screenOrder) {
         return a.screenOrder < b.screenOrder;
      }

      if (a.sortingOrder != b.sortingOrder) {
         return a.sortingOrder < b.sortingOrder;
      }

      return a.hierarchyOrder < b.hierarchyOrder;
   });
   return elements;
}

bool UILayoutSolver::LocalPoint(const UIElementLayout& element, Vector2 point, Vector2& local) {
   const auto& m = element.matrix.m;
   const float determinant = m[0][0] * m[1][1] - m[0][1] * m[1][0];

   if (std::abs(determinant) <= kInverseTolerance) {
      return false;
   }

   const float x = point.x - m[3][0];
   const float y = point.y - m[3][1];
   local = {(x * m[1][1] - y * m[1][0]) / determinant, (y * m[0][0] - x * m[0][1]) / determinant};
   return std::isfinite(local.x) && std::isfinite(local.y);
}

Object* UILayoutSolver::HitTest(
   SceneWorld& world, const std::vector<UIElementLayout>& elements, Vector2 pointer, bool widgetsOnly) {
   for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
      if (!it->visible || (widgetsOnly && !it->interactable)) {
         continue;
      }

      auto* object = world.FindObjectById(it->objectId);

      if (!object || object->GetComponent<UICanvasComponent>() || (widgetsOnly && !GetUIWidget(*object))) {
         continue;
      }

      Vector2 local;

      if (LocalPoint(*it, pointer, local) && local.x >= 0.0f && local.y >= 0.0f && local.x <= it->size.x &&
          local.y <= it->size.y) {
         return object;
      }
   }

   return nullptr;
}

}
