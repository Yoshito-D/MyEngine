#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "MathUtils.h"

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
   int32_t root; ///< ルートジョイントのインデックス
   std::unordered_map<std::string, int32_t> jointMap; ///< 名前からジョイントインデックスへの対応
   std::vector<Joint> joints; ///< 親が子より先に並ぶジョイント列

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
