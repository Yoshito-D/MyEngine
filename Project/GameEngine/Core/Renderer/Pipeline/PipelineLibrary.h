#pragma once
#include <string>
#include <unordered_map>
#include <memory>

namespace GameEngine {

class PipelineState;

struct ComputePipelineDefinition {
   std::string name;
   std::string rootSignatureName;
   std::string computeShaderName;
};

class PipelineLibrary {
public:
   void StoreGraphicsPipeline(const std::string& name, std::unique_ptr<PipelineState> pipeline);
   PipelineState* GetGraphicsPipeline(const std::string& name) const;

   void StoreComputePipeline(const ComputePipelineDefinition& definition);
   const ComputePipelineDefinition* GetComputePipeline(const std::string& name) const;

   void Clear();

private:
   std::unordered_map<std::string, std::unique_ptr<PipelineState>> graphicsPipelines_;
   std::unordered_map<std::string, ComputePipelineDefinition> computePipelines_;
};

}
