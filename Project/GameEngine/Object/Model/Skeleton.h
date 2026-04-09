#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "MathUtils.h"

namespace GameEngine {

struct Joint {
   Transform transform;
   Matrix4x4 localMatrix;
   Matrix4x4 skeletonSpaceMatrix;
   std::string name;
   std::vector<int32_t> children;
   int32_t index;
   std::optional<int32_t> parent;
};

struct Skeleton {
   int32_t root;
   std::unordered_map<std::string, int32_t> jointMap;
   std::vector<Joint> joints;

   void Update() {
      for (Joint& joint : joints) {
         joint.localMatrix = MakeAffineMatrix(joint.transform);
         if (joint.parent) {
            joint.skeletonSpaceMatrix = joint.localMatrix * joints[*joint.parent].skeletonSpaceMatrix;
         } else {
            joint.skeletonSpaceMatrix = joint.localMatrix;
         }
      }
   }
};

}