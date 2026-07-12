#include "ReflectionValidationCoordinator.h"
#include "PSOManager.h"
#include "ShaderManager.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>

#ifdef USE_IMGUI
#include "imgui.h"
#endif

namespace {
struct ValidationGateConfig {
   uint32_t warningThreshold = 5;
   double fallbackRateThreshold = 0.30;
   double minStageMatchRate = 0.60;
   uint32_t warningIncreaseThreshold = 1;
   double fallbackRateIncreaseThreshold = 0.05;
   double stageMatchRateDecreaseThreshold = 0.05;
   std::string source = "internal_default";
};

struct LocalSchemaValidationStatus {
   bool passed = true;
   std::vector<std::string> failedKeys;
   std::string schemaFile;
};

ValidationGateConfig LoadValidationGateConfig() {
   return ValidationGateConfig{};
}

double ComputeStageMatchRate(const GameEngine::PipelineStageMatchInfo& stageInfo) {
   double sumRate = 0.0;
   int stageCount = 0;
   auto accumulate = [&](const GameEngine::ShaderStageMatchInfo& info) {
      if (!info.hasReflection || info.resourceCount == 0) {
         return;
      }
      sumRate += static_cast<double>(info.matchedByName) / static_cast<double>(info.resourceCount);
      ++stageCount;
   };
   accumulate(stageInfo.vertex);
   accumulate(stageInfo.pixel);
   return (stageCount > 0) ? (sumRate / static_cast<double>(stageCount)) : 1.0;
}

LocalSchemaValidationStatus ValidateReportWithSchema(const nlohmann::json& report) {
   LocalSchemaValidationStatus status{};
   status.schemaFile = "internal_schema";

   const nlohmann::json schema = {
      {"type", "object"},
      {"required", nlohmann::json::array({
         "version", "schemaVersion", "compatibilityPolicy", "schema", "schemaValidation", "qualityGate", "diff", "pso", "shader", "renderer"
      })}
   };

   if (!report.is_object()) {
      status.failedKeys.push_back("$report:type");
   }

   for (const auto& req : schema["required"]) {
      const std::string key = req.get<std::string>();
      if (!report.contains(key)) {
         status.failedKeys.push_back("$report." + key);
      }
   }

   status.passed = status.failedKeys.empty();
   return status;
}
}

namespace GameEngine {

void ReflectionValidationCoordinator::BeginFrame(ShaderManager* shaderManager, ReflectionValidationState& state) const {
   if (!shaderManager) {
      return;
   }

   const auto stats = shaderManager->GetResolveStats();
   state.resolveRequestsAtFrameBegin = stats.requests;
   state.resolveHitsAtFrameBegin = stats.hits;
   state.resolveMissesAtFrameBegin = stats.misses;
   state.pipelineStatsAtFrameBegin = shaderManager->GetPipelineResolveStats();
   state.frameStatsByPipeline.clear();
}

void ReflectionValidationCoordinator::EndFrame(ShaderManager* shaderManager, ReflectionValidationState& state) const {
   if (!shaderManager) {
      return;
   }

   const auto stats = shaderManager->GetResolveStats();
   state.frameResolveRequests = stats.requests - state.resolveRequestsAtFrameBegin;
   state.frameResolveHits = stats.hits - state.resolveHitsAtFrameBegin;
   state.frameResolveFallbacks = stats.misses - state.resolveMissesAtFrameBegin;

   const auto currentPipelineStats = shaderManager->GetPipelineResolveStats();
   for (const auto& [pipelineName, current] : currentPipelineStats) {
      const auto beginIt = state.pipelineStatsAtFrameBegin.find(pipelineName);
      const ResolveStats begin = (beginIt != state.pipelineStatsAtFrameBegin.end()) ? beginIt->second : ResolveStats{};
      ResolveStats delta{};
      delta.requests = current.requests - begin.requests;
      delta.hits = current.hits - begin.hits;
      delta.misses = current.misses - begin.misses;
      if (delta.requests > 0) {
         state.frameStatsByPipeline[pipelineName] = delta;
      }
   }
}

void ReflectionValidationCoordinator::UpdateValidationReport(PSOManager* psoManager, ShaderManager* shaderManager, ReflectionValidationState& state) const {
   if (!psoManager || !shaderManager) {
      return;
   }

   const auto psoSummary = psoManager->GetValidationSummary();
   const auto validationMetadata = psoManager->GetAllPipelineReflectionMetadata();
   const auto stageMatchInfos = shaderManager->GetPipelineStageMatchInfos();
   state.latestValidationWarningCount = psoSummary.totalWarnings;
   state.latestFallbackRate = (state.frameResolveRequests > 0)
      ? static_cast<double>(state.frameResolveFallbacks) / static_cast<double>(state.frameResolveRequests)
      : 0.0;

   state.latestQualityGateFailReasons.clear();
   state.latestValidationFailItems.clear();
   state.latestPipelineDiffs.clear();
   state.latestRegressionFailReasons.clear();

   const ValidationGateConfig gateConfig = LoadValidationGateConfig();
   const uint32_t kWarningThreshold = gateConfig.warningThreshold;
   const double kFallbackRateThreshold = gateConfig.fallbackRateThreshold;
   const double kMinStageMatchRate = gateConfig.minStageMatchRate;
   const uint32_t kWarningIncreaseThreshold = gateConfig.warningIncreaseThreshold;
   const double kFallbackRateIncreaseThreshold = gateConfig.fallbackRateIncreaseThreshold;
   const double kStageMatchRateDecreaseThreshold = gateConfig.stageMatchRateDecreaseThreshold;

   std::unordered_map<std::string, uint32_t> currentWarningsByPipeline;
   for (const auto& [pipelineName, metadata] : validationMetadata) {
      currentWarningsByPipeline[pipelineName] = metadata.validationWarningCount;
   }

   std::unordered_map<std::string, double> currentFallbackRateByPipeline;
   for (const auto& [pipelineName, stats] : state.frameStatsByPipeline) {
      const double rate = (stats.requests > 0)
         ? static_cast<double>(stats.misses) / static_cast<double>(stats.requests)
         : 0.0;
      currentFallbackRateByPipeline[pipelineName] = rate;
   }

   std::unordered_map<std::string, double> currentStageMatchRateByPipeline;
   for (const auto& [pipelineName, stageInfo] : stageMatchInfos) {
      currentStageMatchRateByPipeline[pipelineName] = ComputeStageMatchRate(stageInfo);
   }

   nlohmann::json previousReport;
   bool hasPreviousReport = false;
   try {
      std::ifstream ifs("reports/reflection_validation_report.json");
      if (ifs.is_open()) {
         ifs >> previousReport;
         hasPreviousReport = previousReport.is_object();
      }
   } catch (...) {
      hasPreviousReport = false;
   }

   if (state.latestValidationWarningCount > kWarningThreshold) {
      state.latestQualityGateFailReasons.push_back(
         "Validation warnings exceeded threshold (" +
         std::to_string(state.latestValidationWarningCount) + "/" + std::to_string(kWarningThreshold) + ")");
   }
   if (state.latestFallbackRate > kFallbackRateThreshold) {
      state.latestQualityGateFailReasons.push_back(
         "Fallback rate exceeded threshold (" +
         std::to_string(static_cast<int>(state.latestFallbackRate * 100.0)) + "%/" +
         std::to_string(static_cast<int>(kFallbackRateThreshold * 100.0)) + "%)");
   }

   for (const auto& [pipelineName, metadata] : validationMetadata) {
      double stageMatchRate = 1.0;
      if (auto it = stageMatchInfos.find(pipelineName); it != stageMatchInfos.end()) {
         stageMatchRate = ComputeStageMatchRate(it->second);
      }

      if (!metadata.missingSemantics.empty() || stageMatchRate < kMinStageMatchRate) {
         ValidationFailItem item{};
         item.pipeline = pipelineName;
         item.missingSemantics = metadata.missingSemantics;
         item.stageMatchRate = stageMatchRate;
         item.reason = "pipeline=" + pipelineName +
            ", missingSemantics=" + std::to_string(metadata.missingSemantics.size()) +
            ", stageMatchRate=" + std::to_string(stageMatchRate);
         state.latestValidationFailItems.push_back(item);

         state.latestQualityGateFailReasons.push_back(
            "Pipeline " + pipelineName + " failed: missingSemantics=" +
            std::to_string(metadata.missingSemantics.size()) +
            ", stageMatchRate=" + std::to_string(static_cast<int>(stageMatchRate * 100.0)) + "%");
      }
   }

   auto getPreviousPipelineWarning = [&](const std::string& pipelineName) -> uint32_t {
      if (!hasPreviousReport || !previousReport.contains("pso") || !previousReport["pso"].contains("pipelines") || !previousReport["pso"]["pipelines"].is_array()) {
         return 0;
      }
      for (const auto& entry : previousReport["pso"]["pipelines"]) {
         if (!entry.is_object()) {
            continue;
         }
         if (entry.value("name", std::string{}) == pipelineName) {
            return entry.value("validationWarningCount", 0u);
         }
      }
      return 0;
   };

   auto getPreviousPipelineFallbackRate = [&](const std::string& pipelineName) -> double {
      if (!hasPreviousReport || !previousReport.contains("renderer") || !previousReport["renderer"].contains("frameByPipeline")) {
         return 0.0;
      }
      const auto& byPipeline = previousReport["renderer"]["frameByPipeline"];
      if (!byPipeline.contains(pipelineName)) {
         return 0.0;
      }
      return byPipeline[pipelineName].value("fallbackRate", 0.0);
   };

   auto getPreviousPipelineStageMatchRate = [&](const std::string& pipelineName) -> double {
      if (!hasPreviousReport || !previousReport.contains("shader") || !previousReport["shader"].contains("stageMatches")) {
         return 1.0;
      }
      const auto& stageMatches = previousReport["shader"]["stageMatches"];
      if (!stageMatches.contains(pipelineName)) {
         return 1.0;
      }
      return stageMatches[pipelineName].value("averageMatchRate", 1.0);
   };

   nlohmann::json diffByPipeline = nlohmann::json::object();
   for (const auto& [pipelineName, currentWarningCount] : currentWarningsByPipeline) {
      const uint32_t previousWarningCount = getPreviousPipelineWarning(pipelineName);
      const double currentFallbackRate = currentFallbackRateByPipeline.contains(pipelineName) ? currentFallbackRateByPipeline[pipelineName] : 0.0;
      const double previousFallbackRate = getPreviousPipelineFallbackRate(pipelineName);
      const double currentStageRate = currentStageMatchRateByPipeline.contains(pipelineName) ? currentStageMatchRateByPipeline[pipelineName] : 1.0;
      const double previousStageRate = getPreviousPipelineStageMatchRate(pipelineName);

      PipelineDiffMetrics diff{};
      diff.warningDelta = static_cast<int>(currentWarningCount) - static_cast<int>(previousWarningCount);
      diff.fallbackRateDelta = currentFallbackRate - previousFallbackRate;
      diff.stageMatchRateDelta = currentStageRate - previousStageRate;
      state.latestPipelineDiffs[pipelineName] = diff;

      diffByPipeline[pipelineName] = {
         {"warningDelta", diff.warningDelta},
         {"fallbackRateDelta", diff.fallbackRateDelta},
         {"stageMatchRateDelta", diff.stageMatchRateDelta}
      };

      if (hasPreviousReport) {
         if (diff.warningDelta > static_cast<int>(kWarningIncreaseThreshold)) {
            const std::string reason = "Pipeline " + pipelineName + " warning regression: +" + std::to_string(diff.warningDelta);
            state.latestQualityGateFailReasons.push_back(reason);
            state.latestRegressionFailReasons.push_back(reason);
         }
         if (diff.fallbackRateDelta > kFallbackRateIncreaseThreshold) {
            const std::string reason = "Pipeline " + pipelineName + " fallback regression: +" + std::to_string(static_cast<int>(diff.fallbackRateDelta * 100.0)) + "%";
            state.latestQualityGateFailReasons.push_back(reason);
            state.latestRegressionFailReasons.push_back(reason);
         }
         if ((-diff.stageMatchRateDelta) > kStageMatchRateDecreaseThreshold) {
            const std::string reason = "Pipeline " + pipelineName + " stage match regression: " + std::to_string(static_cast<int>(diff.stageMatchRateDelta * 100.0)) + "%";
            state.latestQualityGateFailReasons.push_back(reason);
            state.latestRegressionFailReasons.push_back(reason);
         }
      }
   }

   state.latestQualityGatePassed = state.latestQualityGateFailReasons.empty();

   nlohmann::json report = nlohmann::json::object();
   report["version"] = "1.1.0";
   report["schemaVersion"] = "1.1.0";
   report["compatibilityPolicy"] = {
      {"backwardCompatibleRange", ">=1.0.0 <2.0.0"},
      {"note", "Minor version upgrades remain backward compatible."}
   };
   report["schema"] = {
      {"requiredTopLevelFields", nlohmann::json::array({"version", "schemaVersion", "compatibilityPolicy", "schema", "schemaValidation", "qualityGate", "diff", "pso", "shader", "renderer"})},
      {"qualityGateRequiredFields", nlohmann::json::array({"passed", "warningThreshold", "fallbackRateThreshold", "warningCount", "fallbackRate", "failReasons"})}
   };
   report["diff"] = {
      {"previousReportFound", hasPreviousReport},
      {"warningIncreaseThreshold", kWarningIncreaseThreshold},
      {"fallbackRateIncreaseThreshold", kFallbackRateIncreaseThreshold},
      {"stageMatchRateDecreaseThreshold", kStageMatchRateDecreaseThreshold},
      {"byPipeline", diffByPipeline}
   };
   report["qualityGate"] = {
      {"passed", state.latestQualityGatePassed},
      {"warningThreshold", kWarningThreshold},
      {"fallbackRateThreshold", kFallbackRateThreshold},
      {"minStageMatchRate", kMinStageMatchRate},
      {"warningIncreaseThreshold", kWarningIncreaseThreshold},
      {"fallbackRateIncreaseThreshold", kFallbackRateIncreaseThreshold},
      {"stageMatchRateDecreaseThreshold", kStageMatchRateDecreaseThreshold},
      {"configSource", gateConfig.source},
      {"warningCount", state.latestValidationWarningCount},
      {"fallbackRate", state.latestFallbackRate},
      {"failReasons", state.latestQualityGateFailReasons},
      {"failItems", [&]() {
         nlohmann::json items = nlohmann::json::array();
         for (const auto& item : state.latestValidationFailItems) {
            items.push_back({
               {"pipeline", item.pipeline},
               {"reason", item.reason},
               {"missingSemantics", item.missingSemantics},
               {"stageMatchRate", item.stageMatchRate}
            });
         }
         return items;
      }()}
   };

   report["pso"] = psoManager->BuildValidationReportJson();
   nlohmann::json stageMatchesJson = nlohmann::json::object();
   for (const auto& [pipelineName, stageInfo] : stageMatchInfos) {
      const double averageMatchRate = ComputeStageMatchRate(stageInfo);
      stageMatchesJson[pipelineName] = {
         {"vertex", {
            {"hasReflection", stageInfo.vertex.hasReflection},
            {"resourceCount", stageInfo.vertex.resourceCount},
            {"matchedByName", stageInfo.vertex.matchedByName}
         }},
         {"pixel", {
            {"hasReflection", stageInfo.pixel.hasReflection},
            {"resourceCount", stageInfo.pixel.resourceCount},
            {"matchedByName", stageInfo.pixel.matchedByName}
         }},
         {"averageMatchRate", averageMatchRate}
      };
   }

   const auto resolveStats = shaderManager->GetResolveStats();
   nlohmann::json pipelineResolveStatsJson = nlohmann::json::object();
   for (const auto& [pipelineName, stats] : shaderManager->GetPipelineResolveStats()) {
      pipelineResolveStatsJson[pipelineName] = {
         {"requests", stats.requests},
         {"hits", stats.hits},
         {"misses", stats.misses}
      };
   }

   report["shader"] = {
      {"stageMatches", stageMatchesJson},
      {"resolveStats", {
         {"requests", resolveStats.requests},
         {"hits", resolveStats.hits},
         {"misses", resolveStats.misses}
      }},
      {"pipelineResolveStats", pipelineResolveStatsJson}
   };

   nlohmann::json rendererByPipeline = nlohmann::json::object();
   for (const auto& [pipelineName, stats] : state.frameStatsByPipeline) {
      const double fallbackRate = (stats.requests > 0)
         ? static_cast<double>(stats.misses) / static_cast<double>(stats.requests)
         : 0.0;
      rendererByPipeline[pipelineName] = {
         {"requests", stats.requests},
         {"hits", stats.hits},
         {"fallbacks", stats.misses},
         {"fallbackRate", fallbackRate}
      };
   }
   report["renderer"] = {
      {"frameResolveRequests", state.frameResolveRequests},
      {"frameResolveHits", state.frameResolveHits},
      {"frameFallbacks", state.frameResolveFallbacks},
      {"frameByPipeline", rendererByPipeline}
   };

   report["schemaValidation"] = {
      {"passed", true},
      {"schemaFile", "internal_schema"},
      {"failedKeys", nlohmann::json::array()}
   };
   const auto localSchemaStatus = ValidateReportWithSchema(report);
   state.latestSchemaValidationStatus.passed = localSchemaStatus.passed;
   state.latestSchemaValidationStatus.schemaFile = localSchemaStatus.schemaFile;
   state.latestSchemaValidationStatus.failedKeys = localSchemaStatus.failedKeys;
   report["schemaValidation"] = {
      {"passed", state.latestSchemaValidationStatus.passed},
      {"schemaFile", state.latestSchemaValidationStatus.schemaFile},
      {"failedKeys", state.latestSchemaValidationStatus.failedKeys}
   };

   psoManager->SaveValidationReportJson("resources/engine/reports/pso_validation_report.json");

   try {
      const std::filesystem::path outPath = "reports/reflection_validation_report.json";
      std::filesystem::create_directories(outPath.parent_path());
      std::ofstream ofs(outPath);
      if (ofs.is_open()) {
         ofs << report.dump(2);
      }
   } catch (...) {
   }
}

#ifdef USE_IMGUI
void ReflectionValidationCoordinator::DrawDebugWindow(PSOManager* psoManager, ShaderManager* shaderManager, ReflectionValidationState& state) const {
   if (!psoManager || !shaderManager) {
      return;
   }

   ImGui::Begin("Reflection Binding Debug");
   ImGui::Text("Frame Resolve Requests : %llu", state.frameResolveRequests);
   ImGui::Text("Frame Resolve Hits     : %llu", state.frameResolveHits);
   ImGui::Text("Frame Fallbacks        : %llu", state.frameResolveFallbacks);
   ImGui::Separator();
   ImGui::Text("Quality Gate: %s", state.latestQualityGatePassed ? "PASS" : "FAIL");
   ImGui::Text("Validation Warnings: %u", state.latestValidationWarningCount);
   ImGui::Text("Fallback Rate: %.2f%%", state.latestFallbackRate * 100.0);

   if (!state.latestQualityGateFailReasons.empty()) {
      ImGui::Text("Fail Reasons:");
      for (const auto& reason : state.latestQualityGateFailReasons) {
         ImGui::BulletText("%s", reason.c_str());
      }
   }
   ImGui::Checkbox("Show Failed Pipelines Only", &state.showOnlyFailedItems);

   if (ImGui::BeginTabBar("ReflectionValidationTabs")) {
      if (ImGui::BeginTabItem("Schema Validation")) {
         ImGui::Text("Schema Validation: %s", state.latestSchemaValidationStatus.passed ? "PASS" : "FAIL");
         ImGui::Text("Schema File: %s", state.latestSchemaValidationStatus.schemaFile.c_str());
         for (const auto& key : state.latestSchemaValidationStatus.failedKeys) {
            ImGui::BulletText("%s", key.c_str());
         }
         ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("Regression Diffs")) {
         if (ImGui::BeginTable("ReflectionByPipeline", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Pipeline");
            ImGui::TableSetupColumn("Requests");
            ImGui::TableSetupColumn("Hits");
            ImGui::TableSetupColumn("Fallbacks");
            ImGui::TableSetupColumn("MismatchWarnings");
            ImGui::TableHeadersRow();

            for (const auto& [pipelineName, stats] : state.frameStatsByPipeline) {
               ImGui::TableNextRow();
               ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(pipelineName.c_str());
               const auto diffIt = state.latestPipelineDiffs.find(pipelineName);
               const PipelineDiffMetrics diff = (diffIt != state.latestPipelineDiffs.end()) ? diffIt->second : PipelineDiffMetrics{};
               const char* warningTrend = diff.warningDelta > 0 ? "↑" : (diff.warningDelta < 0 ? "↓" : "→");
               const char* fallbackTrend = diff.fallbackRateDelta > 0.0 ? "↑" : (diff.fallbackRateDelta < 0.0 ? "↓" : "→");
               const char* stageTrend = diff.stageMatchRateDelta > 0.0 ? "↑" : (diff.stageMatchRateDelta < 0.0 ? "↓" : "→");

               ImGui::TableSetColumnIndex(1); ImGui::Text("%llu", stats.requests);
               ImGui::TableSetColumnIndex(2); ImGui::Text("%llu %s", stats.hits, stageTrend);
               ImGui::TableSetColumnIndex(3); ImGui::Text("%llu %s", stats.misses, fallbackTrend);
               ImGui::TableSetColumnIndex(4);
               const auto* metadata = psoManager->GetPipelineReflectionMetadata(pipelineName);
               ImGui::Text("%u %s", metadata ? metadata->validationWarningCount : 0, warningTrend);
            }

            ImGui::EndTable();
         }

         if (!state.latestRegressionFailReasons.empty()) {
            ImGui::Text("Regression Fail Reasons:");
            for (const auto& reason : state.latestRegressionFailReasons) {
               ImGui::BulletText("%s", reason.c_str());
            }
         }
         ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("Current Fail Items")) {
         if (state.latestValidationFailItems.empty()) {
            ImGui::Text("No fail items.");
         } else {
            for (int i = 0; i < static_cast<int>(state.latestValidationFailItems.size()); ++i) {
               const auto& item = state.latestValidationFailItems[i];
               const std::string failLabel = item.pipeline + "##fail_" + std::to_string(i);
               if (ImGui::Selectable(failLabel.c_str(), state.selectedFailItemIndex == i)) {
                  state.selectedFailItemIndex = i;
               }
            }
         }
         ImGui::EndTabItem();
      }

      ImGui::EndTabBar();
   }

   if (ImGui::Button("Dump Root Tables To Log")) {
      shaderManager->LogRootParameterTablesDebug();
   }
   ImGui::End();
}
#endif

}
