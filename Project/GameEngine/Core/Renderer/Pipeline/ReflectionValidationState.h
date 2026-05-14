#pragma once
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "ShaderManager.h"

namespace GameEngine {

struct ValidationFailItem {
   std::string pipeline;
   std::string reason;
   std::vector<std::string> missingSemantics;
   double stageMatchRate = 1.0;
};

struct PipelineDiffMetrics {
   int warningDelta = 0;
   double fallbackRateDelta = 0.0;
   double stageMatchRateDelta = 0.0;
};

struct SchemaValidationStatus {
   bool passed = true;
   std::vector<std::string> failedKeys;
   std::string schemaFile;
};

struct ReflectionValidationState {
   uint64_t resolveRequestsAtFrameBegin = 0;
   uint64_t resolveHitsAtFrameBegin = 0;
   uint64_t resolveMissesAtFrameBegin = 0;

   uint64_t frameResolveRequests = 0;
   uint64_t frameResolveHits = 0;
   uint64_t frameResolveFallbacks = 0;

   std::unordered_map<std::string, ResolveStats> pipelineStatsAtFrameBegin;
   std::unordered_map<std::string, ResolveStats> frameStatsByPipeline;

   uint32_t latestValidationWarningCount = 0;
   double latestFallbackRate = 0.0;
   bool latestQualityGatePassed = true;

   std::vector<std::string> latestQualityGateFailReasons;
   std::vector<ValidationFailItem> latestValidationFailItems;
   std::unordered_map<std::string, PipelineDiffMetrics> latestPipelineDiffs;
   SchemaValidationStatus latestSchemaValidationStatus{};
   std::vector<std::string> latestRegressionFailReasons;

   bool showOnlyFailedItems = false;
   int selectedFailItemIndex = -1;
};

}
