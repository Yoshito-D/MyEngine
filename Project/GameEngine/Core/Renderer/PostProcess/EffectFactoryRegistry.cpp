#include "EffectFactoryRegistry.h"
#include "PostProcess.h"

namespace GameEngine {

/// @brief シリアライズ用クラス名と生成処理を関連付ける。
/// @details JSONはC++型を直接生成できないため、シーンへ保存可能な安定名を新規インスタンスの
///          生成処理へ変換する。同名登録は置換され、登録内容の更新にも利用できる。
/// @param className 保存データとファクトリを結び付けるクラス名
/// @param factory 呼び出すたびに所有権付きインスタンスを返す生成処理
void EffectFactoryRegistry::RegisterFactory(const std::string& className, Factory factory) {
   factories_[className] = std::move(factory);
}

/// @brief 登録済みクラス名から新しいポストプロセスを生成する。
/// @details 未登録名はnullptrとして呼び出し側へ返し、設定ファイル上の未知クラスを診断できるようにする。
///          登録済みの場合はファクトリを都度呼び出すため、返却インスタンスは呼び出しごとに独立する。
/// @param className 生成対象を特定するシリアライズ用クラス名
/// @return 生成したポストプロセス。未登録の場合はnullptr
std::unique_ptr<PostProcess> EffectFactoryRegistry::Create(const std::string& className) const {
   auto it = factories_.find(className);
   if (it == factories_.end()) {
      return nullptr;
   }

   return it->second();
}

}
