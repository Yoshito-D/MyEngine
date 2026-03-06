#pragma once
#include <vector>
#include <string>
#include "Utility/Math/Vector3.h"

// 前方宣言
class Planet;
class Rabbit;
class Star;

namespace GameEngine {
   class Model;
}

/// @brief レベルエディター（惑星・うさぎ・星の配置エディター）
class LevelEditor {
public:
   /// @brief 惑星の種類（Planetクラスと対応）
   enum class PlanetType {
	  Static = 0,        // 静止惑星
	  Orbit = 1,         // 円軌道で移動する惑星
	  Pendulum = 2,      // 振り子のように往復する惑星
	  Wave = 3           // 波のように動く惑星
   };

   /// @brief 惑星の配置データ
   struct PlanetData {
	  GameEngine::Vector3 position;
	  float planetRadius;
	  float gravitationalRadius;
	  int id; // 惑星の識別番号
	  PlanetType type = PlanetType::Static; // 惑星の種類
	  
	  // 移動パラメータ
	  GameEngine::Vector3 orbitCenter;    // 軌道の中心（Orbit用）
	  float orbitRadius;      // 軌道半径（Orbit用）
	  float orbitSpeed;       // 軌道速度（Orbit/Wave用）
	  GameEngine::Vector3 orbitAxis;      // 軌道の回転軸（Orbit用）
	  GameEngine::Vector3 waveDirection;  // 波の方向（Wave用）
	  float waveAmplitude;    // 波の振幅（Wave用）
	  float pendulumAngle;    // 振り子の最大角度（Pendulum用）
   };

   /// @brief うさぎの配置データ
   struct RabbitData {
	  int planetId;          // 所属する惑星のID
	  GameEngine::Vector3 offset;        // 惑星からのオフセット
	  float radius;
   };

   /// @brief 星の配置データ
   struct StarData {
	  int planetId;          // 所属する惑星のID
	  GameEngine::Vector3 offset;        // 惑星からのオフセット
	  float radius;
   };
   
   /// @brief ボックスの配置データ
   struct BoxData {
	  int planetId;          // 所属する惑星のID
	  GameEngine::Vector3 offset;        // 惑星からのオフセット
	  float size;            // ボックスのサイズ
	  float mass;            // ボックスの質量
   };

   /// @brief プレイヤーの配置データ
   struct PlayerData {
	  int planetId;          // 開始する惑星のID
	  GameEngine::Vector3 offset;        // 惑星からのオフセット
   };

   /// @brief レベル全体のデータ
   struct LevelData {
	  std::vector<PlanetData> planets;
	  std::vector<RabbitData> rabbits;
	  std::vector<BoxData> boxes;
	  StarData star;
	  bool hasStarData = false; // 星データがあるかどうか
	  PlayerData player;
	  bool hasPlayerData = false; // プレイヤーデータがあるかどうか
   };

public:
   LevelEditor();
   ~LevelEditor();

   /// @brief エディターUIを表示
   void ShowEditor();

   /// @brief デバッグ描画
   void Draw();

   /// @brief 現在のレベルデータをJSONに保存
   /// @param filePath 保存先ファイルパス
   /// @return 成功したらtrue
   bool SaveToJson(const std::string& filePath);

   /// @brief JSONからレベルデータを読み込み
   /// @param filePath 読み込むファイルパス
   /// @return 成功したらtrue
   bool LoadFromJson(const std::string& filePath);

   /// @brief ホットリロード（ファイルが変更されたら自動的に再読み込み）
   /// @param filePath 監視するファイルパス
   void Update(const std::string& filePath);

   /// @brief レベルデータを取得
   const LevelData& GetLevelData() const { return levelData_; }

   /// @brief レベルデータを設定
   void SetLevelData(const LevelData& data) { levelData_ = data; }

   /// @brief レベルデータをクリア
   void ClearLevelData();

   /// @brief エディターを有効化/無効化
   void SetEnabled(bool enabled) { enabled_ = enabled; }

   /// @brief エディターが有効かどうか
   bool IsEnabled() const { return enabled_; }

   /// @brief レベルデータが変更されたかどうか
   bool IsDataChanged() const { return dataChanged_; }

   /// @brief 変更フラグをリセット
   void ResetDataChangedFlag() { dataChanged_ = false; }

   /// @brief デバッグ描画を有効化/無効化
   void SetDebugDrawEnabled(bool enabled) { debugDrawEnabled_ = enabled; }

   /// @brief デバッグ描画が有効かどうか
   bool IsDebugDrawEnabled() const { return debugDrawEnabled_; }

private:
   // 惑星エディターUI
   void ShowPlanetEditor();
   
   // うさぎエディターUI
   void ShowRabbitEditor();
   
   // 星エディターUI
   void ShowStarEditor();
   
   // ボックスエディターUI
   void ShowBoxEditor();
   
   // プレイヤーエディターUI
   void ShowPlayerEditor();
   
   // ファイル操作UI
   void ShowFileOperations();
   
   // デバッグ描画設定UI
   void ShowDebugDrawSettings();

   // 次の惑星IDを取得
   int GetNextPlanetId() const;

   // 惑星IDから惑星データを取得
   PlanetData* FindPlanetById(int id);
   
   // 惑星のデバッグ描画
   void DrawPlanetDebug(const PlanetData& planet);
   
   // うさぎのデバッグ描画
   void DrawRabbitDebug(const RabbitData& rabbit);
   
   // 星のデバッグ描画
   void DrawStarDebug(const StarData& star);
   
   // ボックスのデバッグ描画
   void DrawBoxDebug(const BoxData& box);

private:
   LevelData levelData_;
   bool enabled_;
   bool dataChanged_;
   bool debugDrawEnabled_;
   
   // 新規追加用の一時データ
   PlanetData newPlanetData_;
   RabbitData newRabbitData_;
   BoxData newBoxData_;
   
   // ファイルパス（保存/読み込み用）
   char filePathBuffer_[256];
   
   // ホットリロード用
   std::string lastLoadedFilePath_;
   long long lastFileWriteTime_;
   
   // デバッグ描画設定
   bool drawPlanetSpheres_;
   bool drawGravityRadius_;
   bool drawOrbitPaths_;
   bool drawLabels_;
   bool drawRabbits_;
   bool drawStar_;
   bool drawBoxes_;
};
