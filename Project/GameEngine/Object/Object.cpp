#include "pch.h"
#include "Object.h"
#include "Camera.h"
#include "Model/Model.h"
#include "ModelAsset.h"

namespace GameEngine {
namespace {
GraphicsDevice* sDevice_ = nullptr;
bool sIsInitialized_ = false;
}

void Object::Initialize(GraphicsDevice* device) {
   if (sIsInitialized_) return;
   sDevice_ = device;
   sIsInitialized_ = true;
}

Material* Object::GetMaterial(size_t index) const {
   if (index >= materials_.size()) {
	  return nullptr;
   }
   return materials_[index];
}

void Object::SetMaterial(Material* material) {
   assert(material != nullptr);
   materials_.clear();
   materials_.push_back(material);
}

void Object::AddMaterial(Material* material) {
   assert(material != nullptr);
   materials_.push_back(material);
}

void Object::SetMaterials(const std::vector<Material*>& materials) {
   assert(!materials.empty());
   // 全てのマテリアルがnullでないことを確認
   for (const auto& material : materials) {
	  assert(material != nullptr);
   }
   materials_ = materials;
}

void Object::CreateTransformationMatrix() {
   transformationMatrix_ = std::make_unique<TransformationMatrix>();
   transformationMatrix_->Create();
}

void Object::CreateMesh() {
   if (createMeshFunc_) {
	  createMeshFunc_();
   }
}
}