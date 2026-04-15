#include "TestScene.h"
#include "Framework/EngineContext.h"
#include "Component/AnimationComponent.h"
#include "MathUtils.h"

using namespace GameEngine;

void TestScene::Initialize() {
   BaseScene::Initialize();

   EngineContext::LoadModel("resources/models/planet", "planet.obj");
   EngineContext::LoadModel("resources/models/plane", "plane.obj");
   EngineContext::LoadModel("resources/models/cube", "AnimatedCube.gltf");
   EngineContext::LoadAnimation("resources/models/cube", "AnimatedCube.gltf");
   EngineContext::LoadModel("resources/models/simpleSkin", "simpleSkin.gltf");
   EngineContext::LoadAnimation("resources/models/simpleSkin", "simpleSkin.gltf");
   EngineContext::LoadModel("resources/models/human", "sneakWalk.gltf");
   EngineContext::LoadAnimation("resources/models/human", "sneakWalk.gltf");
   EngineContext::CreateMaterial("spherePhongMaterial", 0xffffffff, 3);
   EngineContext::CreateMaterial("sphereBlinnPhongMaterial", 0xffffffff, 4);
   EngineContext::CreateMaterial("planeMaterial");
   EngineContext::CreateMaterial("cubeGltfMaterial");
   EngineContext::CreateMaterial("skeletonDebugMaterial");
   EngineContext::LoadTexture("resources/textures/white1x1.png", "white1x1");
   EngineContext::LoadTexture("resources/textures/monsterBall.png", "monsterBall");
   EngineContext::LoadTexture("resources/textures/uvChecker.png", "uvChecker");

   auto spherePhongMaterial = EngineContext::GetMaterial("spherePhongMaterial");
   auto sphereBlinnPhongMaterial = EngineContext::GetMaterial("sphereBlinnPhongMaterial");
   auto planeMaterial = EngineContext::GetMaterial("planeMaterial");
   auto cubeGltfMaterial = EngineContext::GetMaterial("cubeGltfMaterial");
   auto skeletonDebugMaterial = EngineContext::GetMaterial("skeletonDebugMaterial");
   auto sphereModelAsset = EngineContext::GetModel("planet.obj");
   auto planeModelAsset = EngineContext::GetModel("plane.obj");
   auto cubeGltfModelAsset = EngineContext::GetModel("AnimatedCube.gltf");
   auto skeletonModelAsset = EngineContext::GetModel("simpleSkin.gltf");
   auto sneakWalkModelAsset = EngineContext::GetModel("sneakWalk.gltf");

   testSpherePhongModel_ = std::make_unique<Model>();
   testSpherePhongModel_->Create(sphereModelAsset, spherePhongMaterial);
   testSpherePhongModel_->SetPosition(Vector3(-4.0f, 1.0f, 0.0f));

   testSphereBlinnPhongModel_ = std::make_unique<Model>();
   testSphereBlinnPhongModel_->Create(sphereModelAsset, sphereBlinnPhongMaterial);
   testSphereBlinnPhongModel_->SetPosition(Vector3(4.0f, 1.0f, 0.0f));

   testPlaneModel_ = std::make_unique<Model>();
   testPlaneModel_->Create(planeModelAsset, planeMaterial);
   testPlaneModel_->SetRotation(Vector3(ToRadians(90.0f), 0.0f, 0.0f));
   testPlaneModel_->SetScale(Vector3(20.0f, 20.0f, 20.0f));
   testPlaneModel_->SetPosition(Vector3(0.0f, -1.0f, 0.0f));

   testCubeGltfModel_ = std::make_unique<Model>();
   testCubeGltfModel_->Create(cubeGltfModelAsset, cubeGltfMaterial);
   testCubeGltfModel_->SetPosition(Vector3(0.0f, 1.0f, 3.0f));
   if (auto* animationComponent = testCubeGltfModel_->AddComponent<AnimationComponent>()) {
	  animationComponent->animationName = "AnimatedCube.gltf";
	  animationComponent->playing = true;
	  animationComponent->loop = true;
   }

   testSkeletonModel_ = std::make_unique<Model>();
   testSkeletonModel_->Create(skeletonModelAsset, skeletonDebugMaterial);
   testSkeletonModel_->SetPosition(Vector3(0.0f, -1.0f, 0.0f));
   if (auto* animationComponent = testSkeletonModel_->AddComponent<AnimationComponent>()) {
	  animationComponent->animationName = "simpleSkin.gltf";
	  animationComponent->playing = true;
	  animationComponent->loop = true;
      animationComponent->applyTranslation = false;
	  animationComponent->applyRotation = false;
	  animationComponent->applyScale = false;
   }

   testSneakWalkModel_ = std::make_unique<Model>();
   testSneakWalkModel_->Create(sneakWalkModelAsset, skeletonDebugMaterial);
   testSneakWalkModel_->SetPosition(Vector3(2.5f, -1.0f, 0.0f));
   testSneakWalkModel_->SetScale(Vector3(1.0f, 1.0f, 1.0f));
   if (auto* animationComponent = testSneakWalkModel_->AddComponent<AnimationComponent>()) {
	  animationComponent->animationName = "sneakWalk.gltf";
	  animationComponent->playing = true;
	  animationComponent->loop = true;
      animationComponent->applyTranslation = false;
	  animationComponent->applyRotation = false;
	  animationComponent->applyScale = false;
	  animationComponent->useSkinning = true;
   }

#ifdef USE_IMGUI
   isDebugCameraActive_ = true;
#endif
}

void TestScene::Update() {
   BaseScene::Update();

   EngineContext::DebugDrawLights();

   if (testSkeletonModel_) {
	  EngineContext::DrawSkeleton(
		 testSkeletonModel_.get(),
		 0.025f,
		 Vector4(1.0f, 0.2f, 0.2f, 1.0f),
		 Vector4(0.2f, 1.0f, 1.0f, 1.0f),
        false
	  );
   }

   if (testSneakWalkModel_) {
	  EngineContext::DrawSkeleton(
		 testSneakWalkModel_.get(),
		 0.02f,
		 Vector4(1.0f, 0.8f, 0.1f, 1.0f),
		 Vector4(0.4f, 1.0f, 0.2f, 1.0f),
		 false
	  );
   }
}

void TestScene::Draw() {

   BaseScene::Draw();
}
