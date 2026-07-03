#pragma once
#include "AnimationAsset.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace GameEngine {

class AnimationAssetManager {
public:
   /// @brief アニメーションアセットのハンドル型
   using AnimationHandle = std::shared_ptr<AnimationAsset>;

   /// @brief アニメーションアセットをロードする
   /// @param animationPath アニメーションファイルのパス
   /// @param animationName アニメーションの名前
   /// @return アニメーションアセットのハンドル
   AnimationHandle LoadAnimation(const std::string& animationPath, const std::string& animationName);

   /// @brief アニメーションアセットを取得する
   /// @param animationName アニメーションの名前
   /// @return アニメーションアセットのハンドル
   AnimationHandle GetAnimation(const std::string& animationName);

   /// @brief アニメーションアセットを削除する
   void Clear();

   /// @brief 登録されているアニメーション名の一覧を取得する
   std::vector<std::string> GetAnimationNames() const;

private:
   std::unordered_map<std::string, AnimationHandle> animationAssets_;
};

}
