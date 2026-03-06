#pragma once
#include <d3d12.h>
#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include "PostProcess.h"

namespace GameEngine {
class GraphicsDevice;
class OffscreenRenderTarget;
class PSOManager; // 前方宣言のみ

/// @brief ポストプロセス効果の管理と制御を行うクラス
class PostProcessManager {
public:
   /// @brief ポストプロセス効果の情報
   struct EffectInfo {
	  std::unique_ptr<PostProcess> effect;
	  bool enabled = true;
	  int priority = 0; // 数値が小さいほど先に実行される
	  std::string name;
	  std::string pipelineName; // パイプライン名を追加

	  EffectInfo(std::unique_ptr<PostProcess> eff, const std::string& effectName, int prio = 0, const std::string& pipeline = "")
		 : effect(std::move(eff)), name(effectName), priority(prio), pipelineName(pipeline) {}
   };

   /// @brief エフェクト定義構造体
   struct EffectDefinition {
	  std::string name;
	  std::string className;
	  int priority = 0;
	  bool enabled = true;
	  std::string pipelineName; // パイプライン名を追加
   };

   /// @brief 初期化
   /// @param device グラフィックスデバイス
   /// @param renderTarget オフスクリーンレンダーターゲット
   /// @param pipelineManager パイプラインマネージャー（パイプライン取得用）
   void Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget, PSOManager* psoManager);

   /// @brief JSON定義ファイルからエフェクトを読み込み
   /// @param definitionFilePath 定義ファイルのパス
   /// @return 成功時はtrue
   bool LoadEffectsFromJson(const std::wstring& definitionFilePath);

   /// @brief 事前定義されたエフェクトを登録（後方互換性用）
   void RegisterPredefinedEffects();

   /// @brief エフェクトを登録
   /// @param effect ポストプロセス効果
   /// @param name エフェクト名
   /// @param priority 実行優先度（小さいほど先に実行）
   /// @param enabled 初期有効状態
   /// @param pipelineName パイプライン名
   void RegisterEffect(std::unique_ptr<PostProcess> effect, const std::string& name, int priority = 0, bool enabled = true, const std::string& pipelineName = "");

   /// @brief 全てのエフェクトを適用
   /// @param inputSRV 入力SRV
   void ApplyEffects(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);

   /// @brief エフェクトの有効/無効を設定
   /// @param name エフェクト名
   /// @param enabled 有効/無効
   void SetEffectEnabled(const std::string& name, bool enabled);

   /// @brief エフェクトの有効状態を取得
   /// @param name エフェクト名
   /// @return 有効/無効
   bool IsEffectEnabled(const std::string& name) const;

   /// @brief エフェクトの優先度を設定
   /// @param name エフェクト名
   /// @param priority 優先度
   void SetEffectPriority(const std::string& name, int priority);

   /// @brief エフェクトの優先度を取得
   /// @param name エフェクト名
   /// @return 優先度
   int GetEffectPriority(const std::string& name) const;

   /// @brief エフェクトを取得
   /// @param name エフェクト名
   /// @return ポストプロセス効果のポインタ（見つからない場合はnullptr）
   PostProcess* GetEffect(const std::string& name) const;

   /// @brief 登録されているエフェクト名のリストを取得
   /// @return エフェクト名のリスト
   std::vector<std::string> GetEffectNames() const;

   /// @brief エフェクトの実行順序でソートされたエフェクト情報を取得
   /// @return ソートされたエフェクト情報のリスト
   std::vector<const EffectInfo*> GetSortedEffects() const;

   /// @brief エフェクトを削除
   /// @param name エフェクト名
   void RemoveEffect(const std::string& name);

   /// @brief 全てのエフェクトを削除
   void ClearEffects();

   /// @brief 全てのエフェクトを有効にする
   void EnableAllEffects();

   /// @brief 全てのエフェクトを無効にする
   void DisableAllEffects();

#ifdef USE_IMGUI
   /// @brief ImGuiコントロールを表示
   void ShowImGuiControls();
#endif

private:
   GraphicsDevice* device_ = nullptr;
   OffscreenRenderTarget* renderTarget_ = nullptr;
   PSOManager* psoManager_ = nullptr; // パイプラインマネージャーを追加
   std::vector<EffectInfo> effects_;

   /// @brief エフェクトを名前で検索
   /// @param name エフェクト名
   /// @return エフェクト情報のイテレータ
   auto FindEffect(const std::string& name) -> decltype(effects_.begin());
   auto FindEffect(const std::string& name) const -> decltype(effects_.cbegin());

   /// @brief エフェクトリストを優先度でソート
   void SortEffectsByPriority();

   /// @brief クラス名からエフェクトインスタンスを作成
   /// @param className クラス名
   /// @return 作成されたエフェクトのunique_ptr（失敗時はnullptr）
   std::unique_ptr<PostProcess> CreateEffectByClassName(const std::string& className);
};
}