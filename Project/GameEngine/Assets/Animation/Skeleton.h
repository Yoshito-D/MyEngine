#pragma once

#include <optional>
#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

#include "GameEngine/Math/MathUtils.h"

namespace GameEngine {

/// @brief スケルトンを構成する1ジョイントの局所変換と階層情報
struct Joint {
   Transform transform; ///< 親ジョイントから見た局所変換
   Matrix4x4 localMatrix; ///< 局所変換から生成した行列
   Matrix4x4 skeletonSpaceMatrix; ///< スケルトンルートから見た累積行列
   std::string name; ///< アニメーションチャンネルと対応付ける名前
   std::vector<int32_t> children; ///< 子ジョイントのインデックス一覧
   int32_t index; ///< joints配列内のインデックス
   std::optional<int32_t> parent; ///< 親インデックス。ルートの場合は未設定
};

/// @brief 親子順に並んだジョイントと名前検索テーブルを保持する
struct Skeleton {
private:
   int32_t root = -1; ///< ルートジョイントのインデックス
   std::unordered_map<std::string, int32_t> jointMap; ///< 名前からジョイントインデックスへの対応
   std::vector<Joint> joints; ///< 親が子より先に並ぶジョイント列

public:
   /// @brief 親先行順、索引、親子対応、名前の一意性を検証し、階層を一括構築する。
   bool BuildHierarchy(std::vector<Joint> hierarchy, int32_t rootIndex) {
      if (rootIndex < 0 || static_cast<size_t>(rootIndex) >= hierarchy.size()) return false;
      std::unordered_map<std::string, int32_t> names;
      for (size_t i = 0; i < hierarchy.size(); ++i) {
         const auto& joint = hierarchy[i];
         if (joint.index != static_cast<int32_t>(i) || !names.emplace(joint.name, joint.index).second) return false;
         if (joint.parent && (*joint.parent < 0 || static_cast<size_t>(*joint.parent) >= i)) return false;
         if (!joint.parent && static_cast<int32_t>(i) != rootIndex) return false;
         for (const auto child : joint.children) {
            if (child < 0 || static_cast<size_t>(child) >= hierarchy.size() || hierarchy[child].parent != joint.index) return false;
         }
         if (joint.parent) {
            const auto& siblings = hierarchy[*joint.parent].children;
            if (std::count(siblings.begin(), siblings.end(), joint.index) != 1) return false;
         }
      }
      root = rootIndex; joints = std::move(hierarchy); jointMap = std::move(names);
      Update();
      return true;
   }
   /// @brief 構築済みの階層を読み取る。
   const std::vector<Joint>& GetJoints() const { return joints; }
   /// @brief 検証済みの名前索引を読み取る。
   const std::unordered_map<std::string, int32_t>& GetJointMap() const { return jointMap; }
   /// @brief 階層情報を変更せず、指定ジョイントの局所姿勢を適用する。
   bool ApplyJointPose(size_t index, const Transform& pose) {
      if (index >= joints.size()) return false;
      joints[index].transform = pose;
      return true;
   }

   /// @brief 局所変換から全ジョイントのスケルトン空間行列を更新する
   void Update() {
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
};

}
