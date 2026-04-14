#include "PipelineLibrary.h"
#include "Graphics/PipelineState.h"

namespace GameEngine {

void PipelineLibrary::StoreGraphicsPipeline(const std::string& name, std::unique_ptr<PipelineState> pipeline) {
   graphicsPipelines_[name] = std::move(pipeline);
}

PipelineState* PipelineLibrary::GetGraphicsPipeline(const std::string& name) const {
   auto it = graphicsPipelines_.find(name);
   return (it != graphicsPipelines_.end()) ? it->second.get() : nullptr;
}

void PipelineLibrary::StoreComputePipeline(const ComputePipelineDefinition& definition) {
   computePipelines_[definition.name] = definition;
}

const ComputePipelineDefinition* PipelineLibrary::GetComputePipeline(const std::string& name) const {
   auto it = computePipelines_.find(name);
   return (it != computePipelines_.end()) ? &it->second : nullptr;
}

void PipelineLibrary::Clear() {
   graphicsPipelines_.clear();
   computePipelines_.clear();
}

}
