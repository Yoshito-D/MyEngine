#include "PipelineLibrary.h"
#include "Graphics/PipelineState.h"

namespace GameEngine {

/// @brief 名前付きグラフィックスパイプラインの所有権をライブラリへ移す。
/// @details 同名エントリが既にある場合は置き換え、旧パイプラインを解放する。これにより再構築時も
///          呼び出し側は個別の破棄処理を持たずに済む。
/// @param name 検索に使用する一意なパイプライン名
/// @param pipeline 保存するパイプラインステート
void PipelineLibrary::StoreGraphicsPipeline(const std::string& name, std::unique_ptr<PipelineState> pipeline) {
   graphicsPipelines_[name] = std::move(pipeline);
}

/// @brief 名前に対応するグラフィックスパイプラインを参照する。
/// @details 返却ポインタの所有権はライブラリに残り、同名エントリの置換またはClear後は保持できない。
///          未登録を例外にせずnullptrで表すことで、呼び出し側で派生名から基本名への探索を継続できる。
/// @param name 検索するパイプライン名
/// @return 登録済みのパイプライン。該当する名前がなければnullptr
PipelineState* PipelineLibrary::GetGraphicsPipeline(const std::string& name) const {
   auto it = graphicsPipelines_.find(name);
   return (it != graphicsPipelines_.end()) ? it->second.get() : nullptr;
}

/// @brief コンピュートパイプライン定義を名前付きで保存する。
/// @details 定義を値として保持し、ComPtrを通じてPSOの参照を維持する。同名エントリは新しい定義で置き換える。
/// @param definition 識別名、生成元名、生成済みPSOを含む定義
void PipelineLibrary::StoreComputePipeline(const ComputePipelineDefinition& definition) {
   computePipelines_[definition.name] = definition;
}

/// @brief 名前に対応するコンピュートパイプライン定義を参照する。
/// @details 返却値はライブラリ内要素への非所有ポインタであり、同名エントリの置換またはClearをまたいで保持しない。
/// @param name 検索するコンピュートパイプライン名
/// @return 登録済みの定義。該当する名前がなければnullptr
const ComputePipelineDefinition* PipelineLibrary::GetComputePipeline(const std::string& name) const {
   auto it = computePipelines_.find(name);
   return (it != computePipelines_.end()) ? &it->second : nullptr;
}

/// @brief 登録済みのグラフィックス／コンピュートパイプラインをすべて解放する。
/// @details unique_ptrとComPtrが保持している所有権・参照をまとめて破棄し、ライブラリを再構築可能な空状態へ戻す。
void PipelineLibrary::Clear() {
   graphicsPipelines_.clear();
   computePipelines_.clear();
}

}
