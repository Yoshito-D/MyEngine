#pragma once
#include <random>
#include <algorithm>
#include <vector>
#include <concepts>
#include <stdexcept>
#include <cstdint>

namespace GameEngine {
namespace RandomUtils {

/// @brief 乱数エンジンを一括管理する内部クラス
class RandomManager {
public:
   /// @brief 指定された範囲の浮動小数点数を従来と同じ分布で生成する
   template<std::floating_point T>
   static T Uniform(T low, T high) {
      return std::uniform_real_distribution<T>(low, high)(Engine());
   }
   /// @brief 指定された範囲の整数を従来と同じ分布で生成する
   template<std::integral T>
   static T Uniform(T low, T high) {
      return std::uniform_int_distribution<T>(low, high)(Engine());
   }
   /// @brief コレクションを並べ替え、乱数エンジンの所有権は内部に維持する
   template<class Iterator>
   static void Shuffle(Iterator first, Iterator last) { std::shuffle(first, last, Engine()); }
   /// @brief 再現可能な乱数列を明示的に開始する
   static void RestartSequence(uint32_t seed) { Engine().seed(seed); }
private:
   static std::mt19937& Engine() {
      static std::random_device rd;
      static std::mt19937 engine(rd());
      return engine;
   }
};


/// @brief 指定された範囲内のランダムな浮動小数点数を生成
template<std::floating_point T>
T Random(T min, T max) {
   return RandomManager::Uniform(min, max);
}

/// @brief 指定された範囲内のランダムな整数を生成
template<std::integral T>
T Random(T min, T max) {
   return RandomManager::Uniform(min, max);
}

/// @brief ベクターをシャッフルする
template <typename T>
std::vector<T> ShuffleVector(std::vector<T> vec) {
   RandomManager::Shuffle(vec.begin(), vec.end());
   return vec;
}

/// @brief ベクターからランダムに要素を取得
template <typename T>
T GetRandomElement(const std::vector<T>& vec) {
   if (vec.empty()) {
      throw std::runtime_error("Vector is empty");
   }
   return vec[RandomManager::Uniform<size_t>(0, vec.size() - 1)];
}

} // namespace RandomUtils
} // namespace GameEngine