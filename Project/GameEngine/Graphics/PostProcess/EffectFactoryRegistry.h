#pragma once
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace GameEngine {

class PostProcess;

/// @brief クラス名からポストプロセスを復元するためのファクトリ登録表
class EffectFactoryRegistry {
public:
   /// @brief ポストプロセスを生成する呼び出し可能オブジェクト
   using Factory = std::function<std::unique_ptr<PostProcess>()>;

   /// @brief クラス名に対応するファクトリを登録または置換する
   /// @param className シリアライズにも使用する安定したクラス名
   /// @param factory 新しいインスタンスを返すファクトリ
   void RegisterFactory(const std::string& className, Factory factory);
   /// @brief 登録済みクラス名からポストプロセスを生成する
   /// @param className 生成対象のクラス名
   /// @return 生成したエフェクト。未登録の場合はnullptr
   std::unique_ptr<PostProcess> Create(const std::string& className) const;

private:
   std::unordered_map<std::string, Factory> factories_;
};

}
