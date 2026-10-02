#pragma once
#include <random>
#include <algorithm>
#include <vector>
#include <concepts>
#include <stdexcept>

namespace GameEngine {
namespace RandomUtils {

/// @brief 乱数エンジンを一括管理する内部クラス
class RandomManager {
public:
   // シングルトンインスタンスのエンジンを返す
   static std::mt19937& GetEngine() {
      static std::random_device rd;
      static std::mt19937 engine(rd());
      return engine;
   }
};


/// @brief 指定された範囲内のランダムな浮動小数点数を生成
template<std::floating_point T>
T Random(T min, T max) {
   std::uniform_real_distribution<T> dist(min, max);
   return dist(RandomManager::GetEngine());
}

/// @brief 指定された範囲内のランダムな整数を生成
template<std::integral T>
T Random(T min, T max) {
   std::uniform_int_distribution<T> dist(min, max);
   return dist(RandomManager::GetEngine());
}

/// @brief ベクターをシャッフルする
template <typename T>
std::vector<T> ShuffleVector(std::vector<T> vec) {
   std::shuffle(vec.begin(), vec.end(), RandomManager::GetEngine());
   return vec;
}

/// @brief ベクターからランダムに要素を取得
template <typename T>
T GetRandomElement(const std::vector<T>& vec) {
   if (vec.empty()) {
      throw std::runtime_error("Vector is empty");
   }
   std::uniform_int_distribution<size_t> dist(0, vec.size() - 1);
   return vec[dist(RandomManager::GetEngine())];
}

} // namespace RandomUtils
} // namespace GameEngine