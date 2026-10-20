#include "GameEngine/pch.h"
#include "GameEngine/Assets/Animation/Skeleton.h"

namespace GameEngine {

bool Skeleton::BuildHierarchy(std::vector<Joint> hierarchy, int32_t rootIndex) {
   if (rootIndex < 0 || static_cast<size_t>(rootIndex) >= hierarchy.size()) {
      return false;
   }

   std::unordered_map<std::string, int32_t> names;

   for (size_t i = 0; i < hierarchy.size(); ++i) {
      const auto& joint = hierarchy[i];

      if (joint.index != static_cast<int32_t>(i) || !names.emplace(joint.name, joint.index).second) {
         return false;
      }

      if (joint.parent && (*joint.parent < 0 || static_cast<size_t>(*joint.parent) >= i)) {
         return false;
      }

      if (!joint.parent && static_cast<int32_t>(i) != rootIndex) {
         return false;
      }

      for (const auto child : joint.children) {
         if (child < 0 || static_cast<size_t>(child) >= hierarchy.size() || hierarchy[child].parent != joint.index) {
            return false;
         }
      }

      if (joint.parent) {
         const auto& siblings = hierarchy[*joint.parent].children;

         if (std::count(siblings.begin(), siblings.end(), joint.index) != 1) {
            return false;
         }
      }
   }

   root = rootIndex;
   joints = std::move(hierarchy);
   jointMap = std::move(names);
   Update();
   return true;
}

bool Skeleton::ApplyJointPose(size_t index, const Transform& pose) {
   if (index >= joints.size()) {
      return false;
   }

   joints[index].transform = pose;
   return true;
}

void Skeleton::Update() {
   // 親行列を同じループで参照するため、インポート時に保証された親先行順を利用する。
   for (Joint& joint : joints) {
      joint.localMatrix = MakeAffineMatrix(joint.transform);

      if (joint.parent) {
         joint.skeletonSpaceMatrix = joint.localMatrix * joints[*joint.parent].skeletonSpaceMatrix;
      } else {
         joint.skeletonSpaceMatrix = joint.localMatrix;
      }
   }
}

} // namespace GameEngine
