#include "pch.h"
#include "ModelAsset.h"
#include "ResourceHelper.h"
#include <cassert>

namespace {
Logger& log_ = Logger::GetInstance();
}

namespace GameEngine {
void ModelAsset::LoadFile(ID3D12Device* device, const std::string& modelPath, const std::string& modelName) {
   modelData_ = LoadModelFile(modelPath, modelName);

   vertexResources_.resize(modelData_.meshes.size());
   vertexBufferViews_.resize(modelData_.meshes.size());
   mappedVertexData_.resize(modelData_.meshes.size());

   for (size_t i = 0; i < modelData_.meshes.size(); ++i) {
	  const auto& mesh = modelData_.meshes[i];
	  vertexResources_[i] = ResourceHelper::CreateBufferResource(device, sizeof(Mesh::VertexData) * mesh.vertices.size());

	  vertexBufferViews_[i].BufferLocation = vertexResources_[i]->GetGPUVirtualAddress();
	  vertexBufferViews_[i].SizeInBytes = static_cast<UINT>(sizeof(Mesh::VertexData) * mesh.vertices.size());
	  vertexBufferViews_[i].StrideInBytes = sizeof(Mesh::VertexData);

	  vertexResources_[i]->Map(0, nullptr, reinterpret_cast<void**>(&mappedVertexData_[i]));
	  std::memcpy(mappedVertexData_[i], mesh.vertices.data(), sizeof(Mesh::VertexData) * mesh.vertices.size());
   }
}

ModelData ModelAsset::LoadModelFile(const std::string& directoryPath, const std::string& filename) {
   ModelData modelData;

   Assimp::Importer importer;
   std::string filePath = directoryPath + "/" + filename;
   const aiScene* scene = importer.ReadFile(filePath.c_str(),
	  aiProcess_FlipWindingOrder |
	  aiProcess_FlipUVs |
	  aiProcess_Triangulate 
   );
   assert(scene->HasMeshes());

   for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
	  aiMesh* mesh = scene->mMeshes[meshIndex];
	  assert(mesh->HasNormals());

	  MeshData meshData;
	  meshData.materialIndex = mesh->mMaterialIndex;

	  for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
		 aiFace& face = mesh->mFaces[faceIndex];
		 assert(face.mNumIndices == 3);

		 for (uint32_t element = 0; element < face.mNumIndices; ++element) {
			uint32_t vertexIndex = face.mIndices[element];
			aiVector3D& position = mesh->mVertices[vertexIndex];
			aiVector3D& normal = mesh->mNormals[vertexIndex];

			Mesh::VertexData vertex;
			vertex.position = Vector4(-position.x, position.y, position.z, 1.0f);
			vertex.normal = Vector3(-normal.x, normal.y, normal.z);

			if (mesh->HasTextureCoords(0)) {
			   aiVector3D& texCoord = mesh->mTextureCoords[0][vertexIndex];
			   vertex.texCoord = Vector2(texCoord.x, texCoord.y);
			} else {
			   vertex.texCoord = Vector2(0.0f, 0.0f);
			}

			meshData.vertices.push_back(vertex);
		 }
	  }


	  modelData.meshes.push_back(meshData);
   }

   for (uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex) {
	  MaterialAsset materialAsset{};
	  aiMaterial* material = scene->mMaterials[materialIndex];
	  if (material->GetTextureCount(aiTextureType_DIFFUSE) != 0) {
		 aiString textureFilePath;
		 material->GetTexture(aiTextureType_DIFFUSE, 0, &textureFilePath);
		 materialAsset.textureFilePath = directoryPath + "/" + textureFilePath.C_Str();
	  }
	  modelData.materials.push_back(materialAsset);
   }

   modelData.rootNode = ReadNode(scene->mRootNode);

   return modelData;
}

Node ModelAsset::ReadNode(aiNode* node) {
   Node result;
   aiMatrix4x4 aiLocalMatrix = node->mTransformation;
   aiLocalMatrix.Transpose();

   // すべての行列要素をコピー
   for (int row = 0; row < 4; ++row) {
	  for (int col = 0; col < 4; ++col) {
		 result.localMatrix.m[row][col] = aiLocalMatrix[row][col];
	  }
   }

   result.name = node->mName.C_Str();
   result.children.resize(node->mNumChildren);
   for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {
	  result.children[childIndex] = ReadNode(node->mChildren[childIndex]);
   }
   return result;
}
}