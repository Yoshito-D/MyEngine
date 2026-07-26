#pragma once
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "ShaderManager.h"

namespace GameEngine {

/// @brief パイプライン検証に失敗した1件分の診断情報
struct ValidationFailItem {
   std::string pipeline; ///< 対象パイプライン名
   std::string reason; ///< 検証に失敗した理由
   std::vector<std::string> missingSemantics; ///< ルートシグネチャーから解決できなかったセマンティクス
   double stageMatchRate = 1.0; ///< 必要ステージと反映ステージの一致率
};

/// @brief 前回レポートと現在レポートのパイプライン別差分
struct PipelineDiffMetrics {
   int warningDelta = 0; ///< 警告数の増減
   double fallbackRateDelta = 0.0; ///< フォールバック率の増減
   double stageMatchRateDelta = 0.0; ///< ステージ一致率の増減
};

/// @brief 出力レポート自身に対するスキーマ検証結果
struct SchemaValidationStatus {
   bool passed = true; ///< スキーマを満たした場合はtrue
   std::vector<std::string> failedKeys; ///< 不足または不正だったキー
   std::string schemaFile; ///< 検証に使用したスキーマファイル
};

/// @brief リフレクション検証のフレーム計測値・最新結果・UI状態
struct ReflectionValidationState {
   uint64_t resolveRequestsAtFrameBegin = 0; ///< フレーム開始時点の累積解決要求数
   uint64_t resolveHitsAtFrameBegin = 0; ///< フレーム開始時点の累積解決成功数
   uint64_t resolveMissesAtFrameBegin = 0; ///< フレーム開始時点の累積フォールバック数

   uint64_t frameResolveRequests = 0; ///< 現在フレームの解決要求数
   uint64_t frameResolveHits = 0; ///< 現在フレームの解決成功数
   uint64_t frameResolveFallbacks = 0; ///< 現在フレームのフォールバック数

   std::unordered_map<std::string, ResolveStats> pipelineStatsAtFrameBegin; ///< 差分計算用の開始時統計
   std::unordered_map<std::string, ResolveStats> frameStatsByPipeline; ///< 現在フレームのパイプライン別統計

   uint32_t latestValidationWarningCount = 0; ///< 最新検証の警告数
   double latestFallbackRate = 0.0; ///< 最新検証のフォールバック率
   bool latestQualityGatePassed = true; ///< 最新検証が品質基準を満たしたか

   std::vector<std::string> latestQualityGateFailReasons; ///< 品質基準を満たさなかった理由
   std::vector<ValidationFailItem> latestValidationFailItems; ///< 最新の検証失敗項目
   std::unordered_map<std::string, PipelineDiffMetrics> latestPipelineDiffs; ///< 前回レポートとの差分
   SchemaValidationStatus latestSchemaValidationStatus{}; ///< 最新レポートのスキーマ検証結果
   std::vector<std::string> latestRegressionFailReasons; ///< 前回から悪化した指標の説明

   bool showOnlyFailedItems = false; ///< UIで失敗項目だけを表示するか
   int selectedFailItemIndex = -1; ///< UIで選択中の失敗項目。未選択は-1
};

}
