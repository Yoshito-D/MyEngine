#pragma once
#include <string>
#include <cstdint>
#include <vector>
#include <optional>
#include <functional>
#include "GraphicsDevice.h"
#include "Input.h"
#include "Audio.h"
#include "SceneManager.h"
#include "Renderer.h"
#include "AssetManager.h"
#include "TimeProfiler.h"
#include "ModelAsset.h"
#include "CameraManager.h"
#include "LightManager.h"
#include "Utility/JsonDataManager.h"

namespace GameEngine {
class Framework;

class EngineContextInitializer {
private:
   friend class Framework;

   //================================================================
   // 初期化
   //================================================================

   void Initialize(
	  GraphicsDevice* graphicsDevice,
	  Input* input,
	  Audio* audio,
	  Renderer* renderer,
	  AssetManager* assetManager,
	  TimeProfiler* timeProfiler,
	  CameraManager* cameraManager,
	  LightManager* lightManager,
	  JsonDataManager* jsonDataManager
   );

   void SetGraphicsDevice(GraphicsDevice* graphicsDevice);
   void SetInput(Input* input);
   void SetAudio(Audio* audio);
   void SetRenderer(Renderer* renderer);
   void SetAssetManager(AssetManager* assetManager);
   void SetTimeProfiler(TimeProfiler* timeProfiler);
   void SetCameraManager(CameraManager* cameraManager);
   void SetLightManager(LightManager* lightManager);
   void SetJsonDataManager(JsonDataManager* jsonDataManager);
};

class EngineContext {
public:

   //================================================================
   // 入力
   //================================================================

   /// @brief キーが押されているか
   /// @param key キーコード（DIK_で始まる値）
   /// @return キーが押されている場合はtrue
   static bool IsKeyPressed(uint8_t key);

   /// @brief キーが押されているか（enum版）
   /// @param key キーコード
   /// @return キーが押されている場合はtrue
   static bool IsKeyPressed(KeyCode key);

   /// @brief キーが押されていないか
   /// @param key キーコード（DIK_で始まる値）
   /// @return キーが押されていない場合はtrue
   static bool IsKeyNotPressed(uint8_t key);

   /// @brief キーが押されていないか（enum版）
   /// @param key キーコード
   /// @return キーが押されていない場合はtrue
   static bool IsKeyNotPressed(KeyCode key);

   /// @brief キーがトリガー(押された瞬間)されたか
   /// @param key キーコード（DIK_で始まる値）
   /// @return キーがトリガーされた場合はtrue
   static bool IsKeyTriggered(uint8_t key);

   /// @brief キーがトリガー(押された瞬間)されたか（enum版）
   /// @param key キーコード
   /// @return キーがトリガーされた場合はtrue
   static bool IsKeyTriggered(KeyCode key);

   /// @brief キーがリリース(離された瞬間)されたか
   /// @param key キーコード（DIK_で始まる値）
   /// @return キーがリリースされた場合はtrue
   static bool IsKeyReleased(uint8_t key);

   /// @brief キーがリリース(離された瞬間)されたか（enum版）
   /// @param key キーコード
   /// @return キーがリリースされた場合はtrue
   static bool IsKeyReleased(KeyCode key);

   /// @brief マウスのボタンが押されているか
   /// @param button マウスボタン（0:左, 1:右, 2:中ボタン）
   /// @return マウスボタンが押されている場合はtrue
   static bool IsMousePressed(uint8_t button);

   /// @brief マウスのボタンが押されているか（enum版）
   /// @param button マウスボタン
   /// @return マウスボタンが押されている場合はtrue
   static bool IsMousePressed(MouseButton button);

   /// @brief マウスのボタンが押されていないか
   /// @param button マウスボタン（0:左, 1:右, 2:中ボタン）
   /// @return マウスボタンが押されていない場合はtrue
   static bool IsMouseNotPressed(uint8_t button);

   /// @brief マウスのボタンが押されていないか（enum版）
   /// @param button マウスボタン
   /// @return マウスボタンが押されていない場合はtrue
   static bool IsMouseNotPressed(MouseButton button);

   /// @brief マウスのボタンがリリース(離された瞬間)されたか
   /// @param button マウスボタン（0:左, 1:右, 2:中ボタン）
   /// @return マウスボタンがリリースされた場合はtrue
   static bool IsMouseReleased(uint8_t button);

   /// @brief マウスのボタンがリリース(離された瞬間)されたか（enum版）
   /// @param button マウスボタン
   /// @return マウスボタンがリリースされた場合はtrue
   static bool IsMouseReleased(MouseButton button);

   /// @brief マウスのボタンがトリガー(押された瞬間)されたか
   /// @param button マウスボタン（0:左, 1:右, 2:中ボタン）
   /// @return マウスボタンがトリガーされた場合はtrue
   static bool IsMouseTriggered(uint8_t button);

   /// @brief マウスのボタンがトリガー(押された瞬間)されたか（enum版）
   /// @param button マウスボタン
   /// @return マウスボタンがトリガーされた場合はtrue
   static bool IsMouseTriggered(MouseButton button);

   /// @brief マウスのスクリーン座標を取得
   /// @return マウスのスクリーン座標
   static Vector2 GetMouseScreenPosition();

   /// @brief マウスのスクリーン座標を取得
   /// @return マウスのスクリーン座標
   static Vector2 GetMouseDelta();

   /// @brief マウスのホイールの回転量を取得
   /// @return マウスホイールの回転量
   static int32_t GetMouseWheelDelta();

   /// @brief ゲームパッドが接続されているか
   /// @param index ゲームパッド番号（通常0）
   /// @return 接続されていればtrue
   static bool IsGamePadConnected(uint32_t index = 0);

   /// @brief 接続されているゲームパッドの数を取得
   /// @return 現在接続されているゲームパッドの数（最大4）
   static uint32_t GetConnectedGamePadCount();

   /// @brief ゲームパッドのボタンが押されていないか
   /// @param button ゲームパッドのボタンコード（XINPUT_GAMEPAD_で始まる値）
   /// @param index ゲームパッド番号（通常0）
   /// @return ボタンが押されていない場合はtrue
   static bool IsGamePadButtonNotPressed(WORD button, uint32_t index = 0);

   /// @brief ゲームパッドのボタンが押されていないか（enum版）
   /// @param button ゲームパッドのボタン
   /// @param index ゲームパッド番号（通常0）
   /// @return ボタンが押されていない場合はtrue
   static bool IsGamePadButtonNotPressed(GamePadButton button, uint32_t index = 0);

   /// @brief ゲームパッドのボタンが押されているか
   /// @param button ゲームパッドのボタンコード（XINPUT_GAMEPAD_で始まる値）
   /// @param index ゲームパッド番号（通常0）
   /// @return ボタンが押されている場合はtrue
   static bool IsGamePadButtonPressed(WORD button, uint32_t index = 0);

   /// @brief ゲームパッドのボタンが押されているか（enum版）
   /// @param button ゲームパッドのボタン
   /// @param index ゲームパッド番号（通常0）
   /// @return ボタンが押されている場合はtrue
   static bool IsGamePadButtonPressed(GamePadButton button, uint32_t index = 0);

   /// @brief ゲームパッドのボタンがトリガーされたか
   /// @param button ゲームパッドのボタンコード（XINPUT_GAMEPAD_で始まる値）
   /// @param index ゲームパッド番号（通常0）
   /// @return ボタンがトリガーされた場合はtrue
   static bool IsGamePadButtonTriggered(WORD button, uint32_t index = 0);

   /// @brief ゲームパッドのボタンがトリガーされたか（enum版）
   /// @param button ゲームパッドのボタン
   /// @param index ゲームパッド番号（通常0）
   /// @return ボタンがトリガーされた場合はtrue
   static bool IsGamePadButtonTriggered(GamePadButton button, uint32_t index = 0);

   /// @brief ゲームパッドのボタンがリリースされたか
   /// @param button ゲームパッドのボタンコード（XINPUT_GAMEPAD_で始まる値）
   /// @param index ゲームパッド番号（通常0）
   /// @return ボタンがリリースされた場合はtrue
   static bool IsGamePadButtonReleased(WORD button, uint32_t index = 0);

   /// @brief ゲームパッドのボタンがリリースされたか（enum版）
   /// @param button ゲームパッドのボタン
   /// @param index ゲームパッド番号（通常0）
   /// @return ボタンがリリースされた場合はtrue
   static bool IsGamePadButtonReleased(GamePadButton button, uint32_t index = 0);

   /// @brief ゲームパッドのスティックの状態を取得
   /// @param index ゲームパッド番号（通常0）
   /// @param deadZone デッドゾーンの値（0.0～1.0）
   /// @return 左スティックのベクトル
   static Vector2 GetLeftStick(uint32_t index, float deadZone = 0.24);

   /// @brief ゲームパッドの右スティックの状態を取得
   /// @param index ゲームパッド番号（通常0)
   /// @param deadZone デッドゾーンの値（0.0～1.0）
   /// @return 右スティックのベクトル
   static Vector2 GetRightStick(uint32_t index, float deadZone = 0.26f);

   /// @brief ゲームパッドの左トリガーの状態を取得
   /// @param index ゲームパッド番号（通常0）
   /// @param deadZone デッドゾーンの値（0.0～1.0）
   /// @return 左トリガーの値（0.0～1.0）
   static float GetLeftTrigger(uint32_t index, float deadZone = 0.12f);

   /// @brief ゲームパッドの右トリガーの状態を取得
   /// @param index ゲームパッド番号（通常0）
   /// @param deadZone デッドゾーンの値（0.0～1.0）
   /// @return 右トリガーの値（0.0～1.0）
   static float GetRightTrigger(uint32_t index, float deadZone = 0.12f);

   /// @brief ゲームパッドの振動を設定
   /// @param index ゲームパッド番号（通常0）
   /// @param leftMotor 左モーターの強さ（0.0f～1.0f）
   /// @param rightMotor 右モーターの強さ（0.0f～1.0f）
   static void SetVibration(uint32_t index, float leftMotor, float rightMotor);

   //================================================================
   // シーン切り換え
   //================================================================

   /// @brief シーンを変更する
   /// @param name シーン名
   static void ChangeScene(const std::string& name);

   //================================================================
   // タイムプロファイラー
   //================================================================

   /// @brief フレーム時間を取得
   /// @return フレーム時間（秒）
   static float GetFrameTimeMs();

   /// @brief FPSを取得
   /// @return FPS
   static float GetFPS();

   /// @brief デルタタイムを取得
   /// @return デルタタイム（秒）
   static float GetDeltaTime();

   //================================================================
   // アセットマネージャ
   //================================================================

   /// @brief モデルをロードする
   /// @param modelPath モデルのパス
   /// @param modelName モデルの名前
   static void LoadModel(const std::string& modelPath, const std::string& modelName);

   /// @brief モデルを取得する
   /// @param modelName モデルの名前
   static ModelAsset* GetModel(const std::string& modelName);

   /// @brief モデルアセットを全削除
   static void ClearModelAssets();

   /// @brief テクスチャをロードする
   /// @param texturePath テクスチャのパス
   /// @param name 名前
   static void LoadTexture(const std::string& texturePath, const std::string& name);

   /// @brief テクスチャを取得する
   /// @param name 名前
   static Texture* GetTexture(const std::string& name);

   /// @brief テクスチャを全削除
   static void ClearTextures();

   /// @brief マテリアルを作成する
   /// @param name マテリアル名
   /// @param color 色（デフォルトは白）
   /// @param lightingMode ライティングモード（デフォルトはHALFLAMBERT）
   /// @param uvTransform UV変換行列（デフォルトは単位行列）
   /// @return 作成されたマテリアルへのポインタ
   static void CreateMaterial(const std::string& name, uint32_t color = 0xffffffff,
	  int32_t lightingMode = Material::LightingMode::HALFLAMBERT, const Matrix4x4& uvTransform = MakeIdentity4x4());

   /// @brief マテリアルを取得する
   /// @param name マテリアル名
   static Material* GetMaterial(const std::string& name);

   /// @brief マテリアルを全削除
   static void ClearMaterials();

   /// @brief サウンドをロードする
   /// @param soundPath サウンドのパス
   /// @param name 名前
   static void LoadSound(const std::string& soundPath, const std::string& name);

   /// @brief サウンドを取得する
   /// @param name 名前
   static Sound* GetSound(const std::string& name);

   /// @brief サウンドを全削除
   static void ClearSounds();

   //================================================================
   // カメラマネージャー
   //================================================================

   /// @brief アクティブなカメラを取得する
   static Camera* GetActiveCamera();

   /// @brief アクティブなカメラを設定する
   static void SetActiveCamera(size_t index = 0);

   /// @brief カメラを追加する
   static void AddCamera(Camera* camera);

   /// @brief カメラの数を取得する
   static size_t GetCameraCount();

   /// @brief カメラの配列を取得する
   static const std::vector<Camera*>& GetCameras();

   /// @brief カメラを全削除する
   static void ClearCameras();

   //================================================================
   // ライトマネージャー
   //================================================================

   //----------------------------------------------------------------
   // DirectionalLight
   //----------------------------------------------------------------
   
   /// @brief ディレクショナルライトを作成
   /// @param name ライト名
   /// @param color ライトの色（デフォルト：白）
   /// @param direction ライトの向き（デフォルト：下向き）
   /// @param intensity 輝度（デフォルト：1.0f）
   /// @return 作成されたライトへのポインタ
   static DirectionalLight* CreateDirectionalLight(const std::string& name, unsigned int color = 0xffffffff, const Vector3& direction = { 0.0f,-1.0f,0.0f }, float intensity = 1.0f);

   /// @brief ディレクショナルライトを取得する
   /// @param name ライト名
   /// @return ディレクショナルライトへのポインタ（存在しない場合はnullptr）
   static DirectionalLight* GetDirectionalLight(const std::string& name);

   /// @brief ディレクショナルライトを削除
   /// @param name ライト名
   /// @return 削除に成功した場合はtrue
   static bool RemoveDirectionalLight(const std::string& name);

   /// @brief すべてのディレクショナルライトを削除
   static void ClearDirectionalLights();

   /// @brief ディレクショナルライトの名前リストを取得
   /// @return ライト名のリスト
   static std::vector<std::string> GetDirectionalLightNames();

   //----------------------------------------------------------------
   // PointLight
   //----------------------------------------------------------------
   
   /// @brief ポイントライトを作成
   /// @param name ライト名
   /// @param color 色（デフォルト：白）
   /// @param position 位置（デフォルト：原点）
   /// @param intensity 強度（デフォルト：1.0f）
   /// @param radius 半径（デフォルト：2.0f）
   /// @param decay 減衰（デフォルト：0.1f）
   /// @return 作成されたライトへのポインタ
   static PointLight* CreatePointLight(const std::string& name, unsigned int color = 0xffffffff, const Vector3& position = { 0.0f,0.0f,0.0f }, float intensity = 1.0f, float radius = 2.0f, float decay = 0.1f);

   /// @brief ポイントライトを取得する
   /// @param name ライト名
   /// @return ポイントライトへのポインタ（存在しない場合はnullptr）
   static PointLight* GetPointLight(const std::string& name);

   /// @brief ポイントライトを削除
   /// @param name ライト名
   /// @return 削除に成功した場合はtrue
   static bool RemovePointLight(const std::string& name);

   /// @brief ポイントライトの配列を取得する
   static const std::vector<PointLight*>& GetPointLights();

   /// @brief ポイントライトを全削除する
   static void ClearPointLights();

   /// @brief ポイントライトの名前リストを取得
   /// @return ライト名のリスト
   static std::vector<std::string> GetPointLightNames();

   //----------------------------------------------------------------
   // SpotLight
   //----------------------------------------------------------------
   
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
   static SpotLight* CreateSpotLight(const std::string& name, unsigned int color = 0xffffffff, const Vector3& position = Vector3(), float intensity = 1.0f, const Vector3& direction = Vector3(0.0f, -1.0f, 0.0f), float distance = 5.0f, float decay = 0.1f, float cosAngle = 0.7f, float cosFalloffStart = 0.9f);

   /// @brief スポットライトを取得する
   /// @param name ライト名
   /// @return スポットライトへのポインタ（存在しない場合はnullptr）
   static SpotLight* GetSpotLight(const std::string& name);

   /// @brief スポットライトを削除
   /// @param name ライト名
   /// @return 削除に成功した場合はtrue
   static bool RemoveSpotLight(const std::string& name);

   /// @brief スポットライトを全削除する
   static void ClearSpotLights();

   /// @brief スポットライトの名前リストを取得
   /// @return ライト名のリスト
   static std::vector<std::string> GetSpotLightNames();

   //----------------------------------------------------------------
   // AreaLight
   //----------------------------------------------------------------
   
   /// @brief エリアライトを作成
   /// @param name ライト名
   /// @param position 中心座標（デフォルト：原点）
   /// @param normal 照射方向（デフォルト：下向き）
   /// @param tangent ライトの右方向ベクトル（デフォルト：右）
   /// @param size 幅と高さ（デフォルト：5x5）
   /// @param color 光の色（デフォルト：白）
   /// @param intensity 強度（デフォルト：1.0f）
   /// @return 作成されたライトへのポインタ
   static AreaLight* CreateAreaLight(const std::string& name, const Vector3& position = { 0.0f, 0.0f, 0.0f }, const Vector3& normal = { 0.0f, -1.0f, 0.0f }, const Vector3& tangent = { 1.0f, 0.0f, 0.0f }, const Vector2& size = { 5.0f, 5.0f }, const Vector3& color = { 1.0f, 1.0f, 1.0f }, float intensity = 1.0f);

   /// @brief エリアライトを取得する
   /// @param name ライト名
   /// @return エリアライトへのポインタ（存在しない場合はnullptr）
   static AreaLight* GetAreaLight(const std::string& name);

   /// @brief エリアライトを削除
   /// @param name ライト名
   /// @return 削除に成功した場合はtrue
   static bool RemoveAreaLight(const std::string& name);

   /// @brief エリアライトを全削除する
   static void ClearAreaLights();

   /// @brief エリアライトの名前リストを取得
   /// @return ライト名のリスト
   static std::vector<std::string> GetAreaLightNames();

   /// @brief 全てのライトのデバッグ用UIを表示
   static void DebugDrawLights();

   //================================================================
   // レンダラー
   //================================================================

   /// @brief モデルを描画する
   /// @param model 描画するモデル
   /// @param texture テクスチャ
   /// @param blendMode ブレンドモード（std::nulloptの場合は現在設定されているモードを使用）
   /// @param applyPostProcess ポストプロセスを適用するかどうか（デフォルト：true）
   static void Draw(Model* model, Texture* texture, std::optional<BlendMode> blendMode = std::nullopt, bool applyPostProcess = true);

   /// @brief モデルを描画する（複数テクスチャ）
   /// @param model 描画するモデル
   /// @param textures テクスチャ配列
   /// @param blendMode ブレンドモード（std::nulloptの場合は現在設定されているモードを使用）
   /// @param applyPostProcess ポストプロセスを適用するかどうか（デフォルト：true）
   static void Draw(Model* model, const std::vector<Texture*>& textures, std::optional<BlendMode> blendMode = std::nullopt, bool applyPostProcess = true);

   /// @brief スプライトを描画する
   /// @param sprite 描画するスプライト
   /// @param texture テクスチャ
   /// @param blendMode ブレンドモード（std::nulloptの場合は現在設定されているモードを使用）
   /// @param applyPostProcess ポストプロセスを適用するかどうか（デフォルト：true）
   static void Draw(Sprite* sprite, Texture* texture, std::optional<BlendMode> blendMode = std::nullopt, bool applyPostProcess = true);

   /// @brief パーティクルシステムを描画する
   /// @param particleSystem 描画するパーティクルシステム
   /// @param applyPostProcess ポストプロセスを適用するかどうか（デフォルト：true）
   static void Draw(ParticleSystem* particleSystem, bool applyPostProcess = true);

   /// @brief UI用スプライトを描画する
   /// @param sprite 描画するスプライト
   /// @param texture テクスチャ
   /// @param anchorPoint アンカーポイント（描画基準点）
   /// @param blendMode ブレンドモード（std::nulloptの場合は現在設定されているモードを使用）
   /// @param applyPostProcess ポストプロセスを適用するかどうか
   /// @param screenWidth 画面幅（デフォルト：1280）
   /// @param screenHeight 画面高さ（デフォルト：720）
   static void DrawUI(Sprite* sprite, Texture* texture,
	  Sprite::AnchorPoint anchorPoint = Sprite::AnchorPoint::TopLeft,
	  std::optional<BlendMode> blendMode = std::nullopt,
	  bool applyPostProcess = true,
	  uint32_t screenWidth = Window::kResolutionWidth,
	  uint32_t screenHeight = Window::kResolutionHeight
   );

#ifdef USE_IMGUI
   static bool GetIsSceneHovered();
   static bool GetIsDockSpaceVisible();
   static void SetDockSpaceVisible(bool visible);
#endif // USE_IMGUI

   /// @brief 線を描画する
   /// @param start 開始点
   /// @param end 終了点
   /// @param color 色
   /// @param applyPostProcess ポストプロセスを適用するかどうか（デフォルト：true）
   static void DrawLine(const Vector3& start, const Vector3& end, const Vector4& color = Vector4(1.0f, 1.0f, 1.0f, 1.0f), bool applyPostProcess = true);

   /// @brief スプライン曲線を描画する
   /// @param controlPoints 制御点のリスト
   /// @param color 色
   /// @param segmentCount セグメント数
   /// @param applyPostProcess ポストプロセスを適用するかどうか（デフォルト：true）
   static void DrawSpline(const std::vector<Vector3>& controlPoints, const Vector4& color = Vector4(1.0f, 1.0f, 1.0f, 1.0f), size_t segmentCount = 100, bool applyPostProcess = true);

   /// @brief グリッドを描画する
   /// @param plane 描画する平面（デフォルト：XZ平面）
   /// @param gridSize グリッドの間隔（デフォルト：1.0f）
   /// @param thickLineInterval 太い線を描画する間隔（デフォルト：10本ごと）
   /// @param range カメラからの範囲（グリッド数、デフォルト：100）
   /// @param enableFade カメラからの距離に応じてフェードアウトするか（デフォルト：true）
   /// @param fadeDistance フェードアウトを開始する距離（デフォルト：50.0f、enableFadeがtrueの場合のみ有効）
   /// @param applyPostProcess ポストプロセスを適用するかどうか（デフォルト：true）
   static void DrawGrid(GridPlane plane = GridPlane::XZ, float gridSize = 1.0f, int thickLineInterval = 10, int range = 100, bool enableFade = true, float fadeDistance = 50.0f, bool applyPostProcess = true);

   // Shape drawing for particle system debugging
   /// @brief 球を描画する
   /// @param center 中心
   /// @param radius 半径
   /// @param color 色
   /// @param applyPostProcess ポストプロセスを適用するかどうか（デフォルト：true）
   static void DrawSphere(const Vector3& center, float radius, const Vector4& color = Vector4(1.0f, 1.0f, 0.0f, 1.0f), bool applyPostProcess = true);

   /// @brief 半球を描画する
   /// @param center 中心
   /// @param radius 半径
   /// @param up 上方向
   /// @param color 色
   /// @param applyPostProcess ポストプロセスを適用するかどうか（デフォルト：true）
   static void DrawHemisphere(const Vector3& center, float radius, const Vector3& up, const Vector4& color = Vector4(1.0f, 1.0f, 0.0f, 1.0f), bool applyPostProcess = true);

   /// @brief 円錐を描画する
   /// @param apex 円錐の頂点
   /// @param radius 円周の半径
   /// @param height 円錐の高さ
   /// @param direction 円錐の向き
   /// @param color 色
   /// @param applyPostProcess ポストプロセスを適用するかどうか（デフォルト：true）
   static void DrawCone(const Vector3& apex, float radius, float height, const Vector3& direction, const Vector4& color = Vector4(1.0f, 1.0f, 0.0f, 1.0f), bool applyPostProcess = true);

   /// @brief ボックスを描画する
   /// @param center 中心
   /// @param size サイズ
   /// @param color 色
   /// @param applyPostProcess ポストプロセスを適用するかどうか（デフォルト：true）
   static void DrawBox(const Vector3& center, const Vector3& size, const Vector4& color = Vector4(1.0f, 1.0f, 0.0f, 1.0f), bool applyPostProcess = true);

   /// @brief 円を描画する
   /// @param center 中心
   /// @param radius 半径
   /// @param normal 法線
   /// @param color 色
   /// @param applyPostProcess ポストプロセスを適用するかどうか（デフォルト：true）
   static void DrawCircle(const Vector3& center, float radius, const Vector3& normal, const Vector4& color = Vector4(1.0f, 1.0f, 0.0f, 1.0f), bool applyPostProcess = true);

   /// @brief ブレンドモードを設定する（次の描画に使用）
   /// @param blendMode ブレンドモード
   static void SetBlendMode(BlendMode blendMode);

   /// @brief 現在のブレンドモードを取得
   /// @return 現在のブレンドモード
   static BlendMode GetCurrentBlendMode();

   //================================================================
   // ポストプロセス制御
   //================================================================

   /// @brief ポストプロセスを有効/無効にする
   /// @param enabled 有効にする場合はtrue、無効にする場合はfalse
   static void SetPostProcessEnabled(bool enabled);

   /// @brief ポストプロセスが有効かどうかを取得
   /// @return 有効な場合はtrue、無効な場合はfalse
   static bool IsPostProcessEnabled();

   /// @brief 個別のポストプロセスエフェクトを有効/無効にする
   /// @param effectName エフェクト名
   /// @param enabled 有効にする場合はtrue、無効にする場合はfalse
   static void SetPostProcessEffectEnabled(const std::string& effectName, bool enabled);

   /// @brief 個別のポストプロセスエフェクトが有効かどうかを取得
   /// @param effectName エフェクト名
   /// @return 有効な場合はtrue、無効な場合はfalse
   static bool IsPostProcessEffectEnabled(const std::string& effectName);

   /// @brief 利用可能なポストプロセスエフェクト名のリストを取得
   /// @return エフェクト名のリスト
   static std::vector<std::string> GetPostProcessEffectNames();

   //================================================================
   // レンズフレア
   //================================================================

   /// @brief レンズフレアのオクルージョンクエリを開始
   static void BeginLensFlareOcclusionQuery();

   /// @brief レンズフレアのオクルージョンクエリを終了
   static void EndLensFlareOcclusionQuery();

   //================================================================
   // JSONデータマネージャー
   //================================================================

   // グループ操作

   /// @brief グループの存在確認
   /// @param groupName グループ名
   /// @return グループが存在する場合は true
   static bool HasJsonGroup(const std::string& groupName);

   /// @brief グループの削除
   /// @param groupName グループ名
   /// @return 削除に成功した場合は true
   static bool RemoveJsonGroup(const std::string& groupName);

   /// @brief すべてのグループ名を取得
   /// @return グループ名のリスト
   static std::vector<std::string> GetJsonGroupNames();

   /// @brief すべてのデータをクリア
   static void ClearJsonData();

   // データ操作（グループ内）

   /// @brief グループ内の値を設定
   /// @param groupName グループ名
   /// @param key キー
   /// @param value 値
   template<typename T>
   static void SetJsonData(const std::string& groupName, const std::string& key, const T& value);

   /// @brief グループ内の値を取得
   /// @param groupName グループ名
   /// @param key キー
   /// @return 値（存在しない場合は std::nullopt）
   template<typename T>
   static std::optional<T> GetJsonData(const std::string& groupName, const std::string& key);

   /// @brief グループ内の値をデフォルト値付きで取得
   /// @param groupName グループ名
   /// @param key キー
   /// @param defaultValue デフォルト値
   /// @return 値（存在しない場合はデフォルト値）
   template<typename T>
   static T GetJsonDataOr(const std::string& groupName, const std::string& key, const T& defaultValue);

   /// @brief グループ内のキーの存在確認
   /// @param groupName グループ名
   /// @param key キー
   /// @return キーが存在する場合は true
   static bool HasJsonKey(const std::string& groupName, const std::string& key);

   /// @brief グループ内のキーを削除
   /// @param groupName グループ名
   /// @param key キー
   /// @return 削除に成功した場合は true
   static bool RemoveJsonKey(const std::string& groupName, const std::string& key);

   // グループレベルの操作

   /// @brief グループ内のすべてのキーを取得
   /// @param groupName グループ名
   /// @return キーのリスト
   static std::vector<std::string> GetJsonKeys(const std::string& groupName);

   /// @brief グループが空かチェック
   /// @param groupName グループ名
   /// @return グループが空の場合は true
   static bool IsJsonGroupEmpty(const std::string& groupName);

   /// @brief グループのデータ数を取得
   /// @param groupName グループ名
   /// @return データ数
   static size_t GetJsonGroupSize(const std::string& groupName);

   /// @brief グループをクリア
   /// @param groupName グループ名
   static void ClearJsonGroup(const std::string& groupName);

   // ファイル操作

   /// @brief JSON ファイルから読み込み
   /// @param filePath ファイルパス
   /// @return 成功した場合は true
   static bool LoadJsonFile(const std::string& filePath);

   /// @brief JSON ファイルへ保存
   /// @param filePath ファイルパス
   /// @param indent インデント（デフォルト: 4）
   /// @return 成功した場合は true
   static bool SaveJsonFile(const std::string& filePath, int indent = 4);

   // 検索機能

   /// @brief グループ内で条件に合うキーを検索
   /// @param groupName グループ名
   /// @param predicate 条件関数
   /// @return 条件に合うキーのリスト
   template<typename T>
   static std::vector<std::string> FindJsonKeys(const std::string& groupName, std::function<bool(const T&)> predicate);

   /// @brief 条件に合うグループを検索
   /// @param predicate 条件関数
   /// @return 条件に合うグループ名のリスト
   static std::vector<std::string> FindJsonGroups(std::function<bool(const std::string&)> predicate);

private:
   // Internal helper to get JsonDataManager pointer
   static JsonDataManager* GetJsonDataManagerInternal_();
};

// テンプレート関数の実装
template<typename T>
inline void EngineContext::SetJsonData(const std::string& groupName, const std::string& key, const T& value) {
   auto* manager = GetJsonDataManagerInternal_();
   if (manager) {
	  manager->Set(groupName, key, value);
   }
}

template<typename T>
inline std::optional<T> EngineContext::GetJsonData(const std::string& groupName, const std::string& key) {
   auto* manager = GetJsonDataManagerInternal_();
   if (!manager) return std::nullopt;
   return manager->Get<T>(groupName, key);
}

template<typename T>
inline T EngineContext::GetJsonDataOr(const std::string& groupName, const std::string& key, const T& defaultValue) {
   auto* manager = GetJsonDataManagerInternal_();
   if (!manager) return defaultValue;
   return manager->GetOr(groupName, key, defaultValue);
}

template<typename T>
inline std::vector<std::string> EngineContext::FindJsonKeys(const std::string& groupName, std::function<bool(const T&)> predicate) {
   auto* manager = GetJsonDataManagerInternal_();
   if (!manager) return std::vector<std::string>();

   auto group = manager->GetGroup(groupName);
   if (!group.has_value()) return std::vector<std::string>();

   return group->get().FindKeys<T>(predicate);
}

} // namespace GameEngine
