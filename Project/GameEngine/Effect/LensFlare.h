#pragma once  
#include "Core/Window/Window.h"  
#include "Utility/VectorMath.h"  
#include <memory>  
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <string>
#include "../../externals/DirectXTex/d3dx12.h"  
#include "Core/Graphics/Texture.h"
#include "Object/Sprite/Sprite.h"
#include "Core/Graphics/Material.h"

namespace GameEngine {
class GraphicsDevice;
class Camera;
class DirectionalLight;

/// @brief レンズフレアエフェクトクラス
/// @details オクルージョンクエリを使用して、光源の可視性に応じてレンズフレアの強度を動的に変化させる
class LensFlare {
public:
   struct LoadedTexture {
	  std::string name;  // 表示名
	  Texture texture;
   };

   struct FlareElement {
	  std::unique_ptr<Sprite> sprite;
	  std::unique_ptr<Material> material;
	  int textureIndex;      // loadedTextures_のインデックス
	  float distance;        // 光源中心からの距離 (0.0f～1.0f)  
	  float scale;           // スプライトの大きさ（画面高さ基準ピクセル）  
	  float intensity;       // 輝度（0～1） 
	  std::string name;      // 名前
	  bool visible = true;
   };

   struct EditorState {
	  int32_t selectedTextureIndex = 0;
	  char newName[64] = "NewFlare";
	  float newDistance = 0.0f;
	  float newScale = 1.0f;
	  float newIntensity = 1.0f;
   };

   /// @brief レンズフレアシステムの初期化
   /// @param device グラフィックスデバイス
   /// @param width 画面幅
   /// @param height 画面高さ
   /// @param camera 2D描画用カメラ（平行投影、原点中央）
   void Initialize(GraphicsDevice* device, int32_t width = Window::kWindowWidth, int32_t height = Window::kWindowHeight, Camera* camera = nullptr);

   /// @brief テクスチャを追加
   /// @param name テクスチャ名
   /// @param texture テクスチャポインタ
   void AddTexture(const std::string& name, Texture* texture);

   /// @brief フレア要素を作成
   /// @param name 要素名
   /// @param textureIndex テクスチャインデックス
   /// @param color カラー
   /// @param distance 光源中心からの距離
   /// @param scale スケール
   /// @param intensity 強度
   void CreateFlareElement(
	  const std::string& name,
	  int textureIndex,
	  uint32_t color,
	  float distance,
	  float scale,
	  float intensity);

#ifdef USE_IMGUI
   /// @brief ImGuiエディター
   void ImGuiEdit();
#endif

   /// @brief レンズフレアを更新（描画はRendererに任せる）
   /// @param lightWorldPos 光源のワールド座標
   /// @param camera カメラ
   void Update(const Vector3& lightWorldPos, Camera* camera);

   /// @brief 更新済みのフレア要素を取得（描画用）
   /// @return フレア要素のリスト
   const std::vector<FlareElement>& GetFlareElements() const { return flareElements_; }

   /// @brief 可視性係数を取得
   /// @return 可視性係数（0.0～1.0）
   float GetVisibilityFactor() const;

   /// @brief フレア要素のテクスチャを取得
   /// @param element フレア要素
   /// @return テクスチャポインタ
   Texture* GetElementTexture(const FlareElement& element);

   /// @brief オクルージョンクエリの開始（光源描画前に呼ぶ）
   /// @param cmdList コマンドリスト
   /// @param lightWorldPos 光源のワールド座標
   /// @param camera カメラ
   void BeginOcclusionQuery(ID3D12GraphicsCommandList* cmdList, const Vector3& lightWorldPos, Camera* camera);

   /// @brief オクルージョンクエリの終了（光源描画後に呼ぶ）
   /// @param cmdList コマンドリスト
   void EndOcclusionQuery(ID3D12GraphicsCommandList* cmdList);

   /// @brief オクルージョンクエリ結果を取得
   void ResolveOcclusionQuery();

   /// @brief 可視ピクセル数を手動で設定（デバッグ用）
   /// @param pixels 可視ピクセル数
   void SetVisiblePixels(UINT64 pixels) { visiblePixels_ = pixels; }

   /// @brief 可視ピクセル数を取得
   /// @return 可視ピクセル数
   UINT64 GetVisiblePixels() const { return visiblePixels_; }

   /// @brief 最大可視ピクセル数を設定
   /// @param maxPixels 最大可視ピクセル数
   void SetMaxVisiblePixels(UINT64 maxPixels) { maxVisiblePixels_ = maxPixels; }

   /// @brief JSON形式で保存
   /// @param filename ファイル名
   void SaveToJson(const std::string& filename);

   /// @brief JSON形式で読み込み
   /// @param filename ファイル名
   void LoadFromJson(const std::string& filename);

   /// @brief クエリヒープを取得
   /// @return クエリヒープ
   ID3D12QueryHeap* GetQueryHeap() const { return queryHeap_.Get(); }

   /// @brief クエリ結果バッファを取得
   /// @return クエリ結果バッファ
   ID3D12Resource* GetQueryResultBuffer() const { return queryResultBuffer_.Get(); }

private:
   template<typename T>
   using ComPtr = Microsoft::WRL::ComPtr<T>;

   /// @brief オクルージョンクエリ用リソースの作成
   void CreateOcclusionQueryResources();

   /// @brief 光源が画面内にあるかチェック
   /// @param lightScreenPos スクリーン座標での光源位置
   /// @return 画面内にあればtrue
   bool IsLightInScreen(const Vector2& lightScreenPos) const;

   GraphicsDevice* device_ = nullptr;
   Camera* camera_;
   Matrix4x4 viewPortMatrix_ = MakeIdentity4x4();
   Vector2 screenSize_{};
   std::vector<LoadedTexture> loadedTextures_;
   std::vector<FlareElement> flareElements_;
   EditorState editorState_;

   // オクルージョンクエリ関連
   ComPtr<ID3D12QueryHeap> queryHeap_ = nullptr;
   ComPtr<ID3D12Resource> queryResultBuffer_ = nullptr;
   UINT64 visiblePixels_ = 0;
   UINT64 maxVisiblePixels_ = 5000;
   bool isQueryActive_ = false;
};
}