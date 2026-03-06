#pragma once
#include <vector>
#include <memory>
#include <map>
#include <string>
#include "LightDataBuffer.h"

namespace GameEngine {
class DirectionalLight;
class PointLight;
class SpotLight;
class AreaLight;
class LightDataBuffer;

/// @brief ライトマネージャークラス
class LightManager {
public:
   /// @brief 初期化
   void Initialize();

   //================================================================
   // DirectionalLight
   //================================================================
   
   /// @brief ディレクショナルライトを作成
   /// @param name ライト名
   /// @param color ライトの色（デフォルト：白）
   /// @param direction ライトの向き（デフォルト：下向き）
   /// @param intensity 輝度（デフォルト：1.0f）
   /// @return 作成されたライトへのポインタ
   DirectionalLight* CreateDirectionalLight(const std::string& name, unsigned int color = 0xffffffff, const Vector3& direction = { 0.0f,-1.0f,0.0f }, float intensity = 1.0f);

   /// @brief ディレクショナルライトを取得
   /// @param name ライト名
   /// @return ディレクショナルライトへのポインタ（存在しない場合はnullptr）
   DirectionalLight* GetDirectionalLight(const std::string& name) const;

   /// @brief ディレクショナルライトを削除
   /// @param name ライト名
   /// @return 削除に成功した場合はtrue
   bool RemoveDirectionalLight(const std::string& name);

   /// @brief すべてのディレクショナルライトを削除
   void ClearDirectionalLights();

   /// @brief ディレクショナルライトの名前リストを取得
   /// @return ライト名のリスト
   std::vector<std::string> GetDirectionalLightNames() const;

   //================================================================
   // PointLight
   //================================================================
   
   /// @brief ポイントライトを作成
   /// @param name ライト名
   /// @param color 色（デフォルト：白）
   /// @param position 位置（デフォルト：原点）
   /// @param intensity 強度（デフォルト：1.0f）
   /// @param radius 半径（デフォルト：2.0f）
   /// @param decay 減衰（デフォルト：0.1f）
   /// @return 作成されたライトへのポインタ
   PointLight* CreatePointLight(const std::string& name, unsigned int color = 0xffffffff, const Vector3& position = { 0.0f,0.0f,0.0f }, float intensity = 1.0f, float radius = 2.0f, float decay = 0.1f);

   /// @brief ポイントライトを取得
   /// @param name ライト名
   /// @return ポイントライトへのポインタ（存在しない場合はnullptr）
   PointLight* GetPointLight(const std::string& name) const;

   /// @brief ポイントライトを削除
   /// @param name ライト名
   /// @return 削除に成功した場合はtrue
   bool RemovePointLight(const std::string& name);

   /// @brief ポイントライトを全削除
   void ClearPointLights();

   /// @brief ポイントライトの名前リストを取得
   /// @return ライト名のリスト
   std::vector<std::string> GetPointLightNames() const;

   //================================================================
   // SpotLight
   //================================================================
   
   /// @brief スポットライトを作成
   /// @param name ライト名
   /// @param color 色（デフォルト：白）
   /// @param position 位置（デフォルト：原点）
   /// @param intensity 強度（デフォルト：1.0f）
   /// @param direction 方向（デフォルト：下向き）
   /// @param distance 到達距離（デフォルト：5.0f）
   /// @param decay 減衰（デフォルト：0.1f）
   /// @param cosAngle スポットライトの角度の余弦（デフォルト：0.7f）
   /// @param cosFalloffStart スポットライトの減衰開始角度の余弦（デフォルト：0.9f）
   /// @return 作成されたライトへのポインタ
   SpotLight* CreateSpotLight(const std::string& name, unsigned int color = 0xffffffff, const Vector3& position = Vector3(), float intensity = 1.0f, const Vector3& direction = Vector3(0.0f, -1.0f, 0.0f), float distance = 5.0f, float decay = 0.1f, float cosAngle = 0.7f, float cosFalloffStart = 0.9f);

   /// @brief スポットライトを取得
   /// @param name ライト名
   /// @return スポットライトへのポインタ（存在しない場合はnullptr）
   SpotLight* GetSpotLight(const std::string& name) const;

   /// @brief スポットライトを削除
   /// @param name ライト名
   /// @return 削除に成功した場合はtrue
   bool RemoveSpotLight(const std::string& name);

   /// @brief スポットライトを全削除
   void ClearSpotLights();

   /// @brief スポットライトの名前リストを取得
   /// @return ライト名のリスト
   std::vector<std::string> GetSpotLightNames() const;

   //================================================================
   // AreaLight
   //================================================================
   
   /// @brief エリアライトを作成
   /// @param name ライト名
   /// @param position 中心座標（デフォルト：原点）
   /// @param normal 照射方向（デフォルト：下向き）
   /// @param tangent ライトの右方向ベクトル（デフォルト：右）
   /// @param size 幅と高さ（デフォルト：5x5）
   /// @param color 光の色（デフォルト：白）
   /// @param intensity 強度（デフォルト：1.0f）
   /// @return 作成されたライトへのポインタ
   AreaLight* CreateAreaLight(const std::string& name, const Vector3& position = { 0.0f, 0.0f, 0.0f }, const Vector3& normal = { 0.0f, -1.0f, 0.0f }, const Vector3& tangent = { 1.0f, 0.0f, 0.0f }, const Vector2& size = { 5.0f, 5.0f }, const Vector3& color = { 1.0f, 1.0f, 1.0f }, float intensity = 1.0f);

   /// @brief エリアライトを取得
   /// @param name ライト名
   /// @return エリアライトへのポインタ（存在しない場合はnullptr）
   AreaLight* GetAreaLight(const std::string& name) const;

   /// @brief エリアライトを削除
   /// @param name ライト名
   /// @return 削除に成功した場合はtrue
   bool RemoveAreaLight(const std::string& name);

   /// @brief エリアライトを全削除
   void ClearAreaLights();

   /// @brief エリアライトの名前リストを取得
   /// @return ライト名のリスト
   std::vector<std::string> GetAreaLightNames() const;

   //================================================================
   // Buffer & Debug
   //================================================================
   
   /// @brief LightDataBufferを取得
   /// @return LightDataBufferのポインタ
   LightDataBuffer* GetLightDataBuffer() const { return lightDataBuffer_.get(); }

   /// @brief 構造化バッファを更新
   void UpdateStructureBuffer();

   /// @brief デバッグ用描画（ImGuiによる編集・追加・削除）
   void DebugDraw();

private:
   std::map<std::string, std::unique_ptr<DirectionalLight>> directionalLights_;
   std::map<std::string, std::unique_ptr<PointLight>> pointLights_;
   std::map<std::string, std::unique_ptr<SpotLight>> spotLights_;
   std::map<std::string, std::unique_ptr<AreaLight>> areaLights_;

   std::unique_ptr<LightDataBuffer> lightDataBuffer_ = nullptr;
};
}
