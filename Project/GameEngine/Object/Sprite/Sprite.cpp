#include "pch.h"
#include "Sprite.h"
#include "Texture.h"
#include "Component/MaterialComponent.h"
#include "Component/MeshComponent.h"
#include "Component/TransformComponent.h"
#include "Component/RenderComponent.h"
#include "Core/Graphics/Mesh.h"
#include "Core/Graphics/TransformationMatrix.h"
#include <algorithm>

namespace GameEngine {

namespace {
// MaterialManagerの名前検索で別Spriteの既定Materialを共有しないよう、プロセス中に重複しない自動名を割り当てる。
uint64_t sAutoSpriteMaterialCounter = 0;

std::string BuildAutoSpriteMaterialName() {
   return "SpriteMaterial_" + std::to_string(++sAutoSpriteMaterialCounter);
}

std::string BuildDefaultSpriteName(const std::vector<Sprite*>& registeredSprites) {
   // 生存中の表示名と衝突しない最小番号を選び、削除済み番号は再利用して名前の無制限な増加を避ける。
   auto exists = [&registeredSprites](const std::string& name) {
	  for (const auto* sprite : registeredSprites) {
		 if (sprite && sprite->GetObjectName() == name) {
			return true;
		 }
	  }
	  return false;
   };

   uint32_t index = 1;
   while (true) {
	  const std::string candidate = "Sprite_" + std::to_string(index++);
	  if (!exists(candidate)) {
		 return candidate;
	  }
   }
}
}

Sprite::Sprite() {
   // AddComponentは同型があれば既存実体を返すため、コンストラクターとCreateの両経路から安全に必須構成を保証できる。
   // MeshはCreateまで形状を持たないが、MaterialとRender設定は生成直後から編集・シリアライズ可能にしておく。
   auto* transformComponent = AddComponent<TransformComponent>();
   transformComponent->transform.scale = Vector3(1.0f, 1.0f, 1.0f);
   if (auto* materialComponent = AddComponent<MaterialComponent>()) {
      materialComponent->EnsureMaterial(BuildAutoSpriteMaterialName(), 0xffffffff, Material::LightingMode::NONE);
   }
   AddComponent<MeshComponent>();
   if (auto* renderComponent = AddComponent<RenderComponent>()) {
      renderComponent->renderSpace = RenderComponent::RenderSpace::Screen;
   }
   SetObjectName(BuildDefaultSpriteName(sRegisteredSprites_));
   // Rendererが所有権を持たずに自動描画対象を列挙するため、構築完了後のthisを静的一覧へ登録する。
   sRegisteredSprites_.push_back(this);
}

Sprite::~Sprite() {
   // Object基底がComponentを破棄する前に、Rendererから参照される非所有ポインターを外す。
   UnregisterSprite(this);
}

void Sprite::UnregisterSprite(Sprite* sprite) {
   if (!sprite) {
	  return;
   }

   // Editorは描画一覧から先に外して実体を遅延破棄するため、デストラクターから再度呼ばれても何もしない冪等操作とする。
   auto it = std::find(sRegisteredSprites_.begin(), sRegisteredSprites_.end(), sprite);
   if (it != sRegisteredSprites_.end()) {
	  sRegisteredSprites_.erase(it);
   }
}

const std::vector<Sprite*>& Sprite::GetRegisteredSprites() {
   // 一覧は所有権を持たず、Spriteのデストラクターまたは明示的なUnregisterが寿命を同期する。
   return sRegisteredSprites_;
}

void Sprite::Create(const Vector2& size, Material* material, const Vector2& anchorPoint) {
   // MeshComponentをQuadの設定元に統一し、アンカー・反転・保存データとGPU頂点が同じ値から再構築されるようにする。
   if (auto* primitiveMeshComponent = AddComponent<MeshComponent>()) {
      primitiveMeshComponent->SetPrimitiveType(MeshComponent::PrimitiveType::Quad);
      primitiveMeshComponent->SetQuadSize(size);
      primitiveMeshComponent->SetQuadAnchorPoint(anchorPoint);
      primitiveMeshComponent->CreateMesh();
   }

   if (material) {
      // nullptrならコンストラクターで作成したSprite専用Materialを維持し、明示指定時だけ外部Materialへ差し替える。
      if (auto* materialComponent = GetComponent<MaterialComponent>()) {
		 materialComponent->AssignMaterial(material);
	  }
   }

   if (auto* renderComponent = AddComponent<RenderComponent>()) {
      renderComponent->renderSpace = RenderComponent::RenderSpace::Screen;
   }

   auto* transformComponent = GetComponent<TransformComponent>();
   if (transformComponent) {
	  transformComponent->transform.scale = { 1.0f, 1.0f, 1.0f };
	  // 既定のUIカメラのNear=0より奥へ置き、生成直後からクリップ範囲内の深度を持たせる。
	  transformComponent->transform.translation.z = 1.0f;
	  // 描画時までGPU定数バッファを遅延できる設計だが、Create済みSpriteは即座に取得可能な状態へする。
	  transformComponent->EnsureTransformationMatrix();
   }

   UpdateVertexPositions();
}

void Sprite::SetAnchorPoint(const Vector2& anchorPoint) {
   if (auto* primitiveMeshComponent = GetMeshComponent()) {
      primitiveMeshComponent->SetQuadAnchorPoint(anchorPoint);
      // Mesh作成済みなら、永続MapされたUPLOADヒープへ4頂点の位置だけを即時反映する。
      primitiveMeshComponent->ApplyToMesh();
   }
}

void Sprite::SetSize(const Vector2& size) {
   if (auto* primitiveMeshComponent = GetMeshComponent()) {
      primitiveMeshComponent->SetQuadSize(size);
      primitiveMeshComponent->ApplyToMesh();
   }
}

void Sprite::SetScale(const Vector2& scale) {
   auto* transformComponent = GetComponent<TransformComponent>();
   if (!transformComponent) {
	  return;
   }
   transformComponent->transform.scale.x = scale.x;
   transformComponent->transform.scale.y = scale.y;
   // Quadに厚みはないが、2D入力によって未指定のZ軸まで0へ落とさないよう1を維持する。
   transformComponent->transform.scale.z = 1.0f;
}

void Sprite::SetPosition(const Vector2& position) {
   auto* transformComponent = GetComponent<TransformComponent>();
   if (!transformComponent) {
	  return;
   }
   transformComponent->transform.translation.x = position.x;
   transformComponent->transform.translation.y = position.y;
   // 既定のScreen描画でUIカメラのクリップ範囲内に収まる深度を維持する。
   transformComponent->transform.translation.z = 1.0f;
}

void Sprite::SetRotation(float rotation) {
   auto* transformComponent = GetComponent<TransformComponent>();
   if (!transformComponent) {
	  return;
   }
   // Quaternion setter経由でEuler側も同期し、描画・Inspector・シリアライズが同じZ回転を参照するようにする。
   transformComponent->transform.SetRotationQuaternion(Vector3(0.0f, 0.0f, rotation).ToQuaternion().Normalize());
}

Vector2 Sprite::GetScale() const {
   const auto* transformComponent = GetComponent<TransformComponent>();
   if (!transformComponent) {
	  return Vector2(1.0f, 1.0f);
   }
   return Vector2{ transformComponent->transform.scale.x, transformComponent->transform.scale.y };
}

Vector2 Sprite::GetPosition() const {
   const auto* transformComponent = GetComponent<TransformComponent>();
   if (!transformComponent) {
	  return Vector2(0.0f, 0.0f);
   }
   return Vector2{ transformComponent->transform.translation.x, transformComponent->transform.translation.y };
}

float Sprite::GetRotation() const {
   const auto* transformComponent = GetComponent<TransformComponent>();
   if (!transformComponent) {
	  return 0.0f;
   }
   return transformComponent->transform.GetActiveEuler().z;
}

Vector2 Sprite::GetSize() const {
   if (const auto* primitiveMeshComponent = GetMeshComponent()) {
      return primitiveMeshComponent->GetQuadSize();
   }
   return Vector2(1.0f, 1.0f);
}

Vector2 Sprite::GetAnchorPoint() const {
   if (const auto* primitiveMeshComponent = GetMeshComponent()) {
      return primitiveMeshComponent->GetQuadAnchorPoint();
   }
   return Vector2(0.0f, 0.0f);
}

bool Sprite::IsFlipX() const {
   if (const auto* primitiveMeshComponent = GetMeshComponent()) {
      return primitiveMeshComponent->IsFlipX();
   }
   return false;
}

bool Sprite::IsFlipY() const {
   if (const auto* primitiveMeshComponent = GetMeshComponent()) {
      return primitiveMeshComponent->IsFlipY();
   }
   return false;
}

void Sprite::SetFlipX(bool isFlip) {
   if (auto* primitiveMeshComponent = GetMeshComponent()) {
      primitiveMeshComponent->SetFlipX(isFlip);
      primitiveMeshComponent->ApplyToMesh();
   }
}

void Sprite::SetFlipY(bool isFlip) {
   if (auto* primitiveMeshComponent = GetMeshComponent()) {
      primitiveMeshComponent->SetFlipY(isFlip);
      primitiveMeshComponent->ApplyToMesh();
   }
}

void Sprite::SetTextureUV(const Vector2& leftTop, const Vector2& size) {
   if (auto* materialComponent = GetMaterialComponent()) {
      // ピクセル矩形はMaterialComponentへ保存し、実テクスチャ寸法が分かる描画更新時に0～1 UVへ変換する。
      materialComponent->SetTextureUV(leftTop, size);
   }
}

void Sprite::SetTextureLeftTop(const Vector2& leftTop) {
   if (auto* materialComponent = GetMaterialComponent()) {
      materialComponent->SetTextureLeftTop(leftTop);
   }
}

void Sprite::SetTextureSize(const Vector2& size) {
   if (auto* materialComponent = GetMaterialComponent()) {
      materialComponent->SetTextureSize(size);
   }
}

Vector2 Sprite::GetTextureLeftTop() const {
   if (const auto* materialComponent = GetMaterialComponent()) {
      return materialComponent->GetTextureLeftTop();
   }
   return Vector2(0.0f, 0.0f);
}

Vector2 Sprite::GetTextureSize() const {
   if (const auto* materialComponent = GetMaterialComponent()) {
      return materialComponent->GetTextureSize();
   }
   return Vector2(0.0f, 0.0f);
}

Mesh* Sprite::GetMesh() const {
   if (const auto* primitiveMeshComponent = GetMeshComponent()) {
      return primitiveMeshComponent->GetMesh();
   }
   return nullptr;
}

TransformationMatrix* Sprite::GetTransformationMatrix() {
   auto* transformComponent = GetTransformComponent();
   if (!transformComponent) {
      return nullptr;
   }
   // CPUから直接更新できる永続Map済み定数バッファを、実際に要求された時点で初めて確保する。
   return transformComponent->EnsureTransformationMatrix();
}

void Sprite::Update(Camera* camera, Texture* texture) {
   auto* transformComponent = GetTransformComponent();
	if (!transformComponent || !camera) {
	   return;
	}

   auto* transformationMatrix = transformComponent->EnsureTransformationMatrix();
   if (!transformationMatrix) {
      return;
   }

   // Quadの設定や描画Textureはフレーム間で変わり得るため、DrawCommand作成前にMap済み頂点を同期する。
   UpdateVertexPositions();
   UpdateTextureCoordinates(texture);

   // 行ベクトル規約に従いLocalの後へParentを合成し、階層化されたSpriteにも同じ描画経路を使う。
   Matrix4x4 worldMatrix = MakeAffineMatrix(transformComponent->transform);
   if (transformComponent->useParentMatrix) {
	  worldMatrix = worldMatrix * transformComponent->parentMatrix;
   }
   Matrix4x4 wVPMatrix = worldMatrix * camera->GetViewProjectionMatrix();
   // TransformationMatrixは永続Mapされており、この書込みが同フレームの頂点シェーダー定数へ直接反映される。
   // 共通Objectシェーダーのレイアウトに合わせ、2D SpriteでもWorldと法線用逆転置を揃えて更新する。
   transformationMatrix->GetTransformationMatrixData()->wVP = wVPMatrix;
   transformationMatrix->GetTransformationMatrixData()->world = worldMatrix;
   transformationMatrix->GetTransformationMatrixData()->worldInverseTranspose = worldMatrix.Inverse().Transpose();
}

void Sprite::UpdateMatrixForUI(Camera* camera, Texture* texture, AnchorPoint anchorPoint, uint32_t screenWidth, uint32_t screenHeight) {
   auto* transformComponent = GetTransformComponent();
	if (!transformComponent || !camera) {
	   return;
	}

   auto* transformationMatrix = transformComponent->EnsureTransformationMatrix();
   if (!transformationMatrix) {
      return;
   }

   // 保存されたQuad設定と現在のTexture寸法を使い、アンカー計算より先に頂点位置とUVを同期する。
   UpdateVertexPositions();
   UpdateTextureCoordinates(texture);


   // UIカメラは画面中心原点・上向きYなので、論理画面の半幅・半高から選択アンカーの基準位置を求める。
   Vector3 anchorPos = CalculateAnchorPosition(anchorPoint, screenWidth, screenHeight);

   // 描画ごとの画面アンカーを永続Transformへ書き戻すと呼出しのたびにOffsetが累積するため、一時コピーだけを調整する。
   Transform finalTransform = transformComponent->transform;
   finalTransform.translation.x += anchorPos.x;
   finalTransform.translation.y += anchorPos.y;
 finalTransform.translation.z = transformComponent->transform.translation.z;

   // 画面アンカーを適用したLocalの後へParentを掛け、通常更新と同じ階層規約を保つ。
   Matrix4x4 worldMatrix = MakeAffineMatrix(finalTransform);
   if (transformComponent->useParentMatrix) {
	  worldMatrix = worldMatrix * transformComponent->parentMatrix;
   }

   Matrix4x4 wVPMatrix = worldMatrix * camera->GetViewProjectionMatrix();
   // 描画キューが参照する定数バッファへ、今回のアンカーを含む行列一式をまとめて反映する。
   transformationMatrix->GetTransformationMatrixData()->wVP = wVPMatrix;
   transformationMatrix->GetTransformationMatrixData()->world = worldMatrix;
   transformationMatrix->GetTransformationMatrixData()->worldInverseTranspose = worldMatrix.Inverse().Transpose();
}

Vector3 Sprite::CalculateAnchorPosition(AnchorPoint anchorPoint, uint32_t screenWidth, uint32_t screenHeight) const {
   Vector3 position = { 0.0f, 0.0f, 0.0f };

   // 入力寸法はピクセルそのものではなくRendererが選んだ論理UI領域で、中心から各辺までの距離へ変換する。
   float halfWidth = screenWidth * 0.5f;
   float halfHeight = screenHeight * 0.5f;

   switch (anchorPoint) {
	  case AnchorPoint::TopLeft:
		 position.x = -halfWidth;
		 position.y = halfHeight;  // 上部なのでプラス
		 break;
	  case AnchorPoint::TopCenter:
		 position.x = 0.0f;
		 position.y = halfHeight;  // 上部なのでプラス
		 break;
	  case AnchorPoint::TopRight:
		 position.x = halfWidth;
		 position.y = halfHeight;  // 上部なのでプラス
		 break;
	  case AnchorPoint::MiddleLeft:
		 position.x = -halfWidth;
		 position.y = 0.0f;
		 break;
	  case AnchorPoint::MiddleCenter:
		 position.x = 0.0f;
		 position.y = 0.0f;
		 break;
	  case AnchorPoint::MiddleRight:
		 position.x = halfWidth;
		 position.y = 0.0f;
		 break;
	  case AnchorPoint::BottomLeft:
		 position.x = -halfWidth;
		 position.y = -halfHeight;  // 下部なのでマイナス
		 break;
	  case AnchorPoint::BottomCenter:
		 position.x = 0.0f;
		 position.y = -halfHeight;  // 下部なのでマイナス
		 break;
	  case AnchorPoint::BottomRight:
		 position.x = halfWidth;
		 position.y = -halfHeight;  // 下部なのでマイナス
		 break;
   }

   return position;
}

void Sprite::UpdateVertexPositions() {
   if (auto* primitiveMeshComponent = GetMeshComponent()) {
      // MeshComponentを正本として、サイズ・頂点アンカー・反転状態をMap済みGPU頂点へ再適用する。
      primitiveMeshComponent->ApplyToMesh();
   }
}

void Sprite::UpdateTextureCoordinates(Texture* texture) {
   auto* primitiveMeshComponent = GetMeshComponent();
   if (!primitiveMeshComponent) {
      return;
   }

   // Material側のピクセル矩形をTextureの実寸で正規化する。Texture未解決時は既存UVを壊さず更新を見送る。
   primitiveMeshComponent->ApplyTextureCoordinates(texture, GetTextureLeftTop(), GetTextureSize());
}

MeshComponent* Sprite::GetMeshComponent() {
   return GetComponent<MeshComponent>();
}

const MeshComponent* Sprite::GetMeshComponent() const {
   return GetComponent<MeshComponent>();
}

MaterialComponent* Sprite::GetMaterialComponent() {
   return GetComponent<MaterialComponent>();
}

const MaterialComponent* Sprite::GetMaterialComponent() const {
   return GetComponent<MaterialComponent>();
}

TransformComponent* Sprite::GetTransformComponent() {
   return GetComponent<TransformComponent>();
}

const TransformComponent* Sprite::GetTransformComponent() const {
   return GetComponent<TransformComponent>();
}

}
