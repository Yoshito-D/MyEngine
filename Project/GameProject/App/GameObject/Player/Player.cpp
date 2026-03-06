#include "Player.h"
#include "../../Collider/SphereCollider.h"
#include "../../Collider/CollisionLayer.h"
#include "../../Camera/TPSCameraController.h"
#include "../../Component/CameraInputHelper.h"
#include "../../Component/PlanetSwitchHelper.h"
#include "../Rabbit/Rabbit.h"
#include "../Box/Box.h"
#include "Collision/Collision.h"
#include "Framework/EngineContext.h"
#include <cmath>
#include <numbers>
#include <algorithm> 
#ifdef USE_IMGUI
#include "UI/ImGuiManager.h"
#include "../../../externals/imgui/imgui.h"
#endif

using namespace GameEngine;

void Player::Initialize(GameEngine::Model* model, float radius) {

   model_ = model;
   playerRadius_ = radius;

   model_->SetUseQuaternion(true); // Quaternion回転を使用

   // コライダー作成
   collider_ = std::make_unique<SphereCollider>(this, playerRadius_);
   collider_->SetLayer(CollisionLayer::Player);
   
   // スピン用コライダー作成（通常の1.5倍の半径、初期状態では無効）
   spinCollider_ = std::make_unique<SphereCollider>(this, playerRadius_ * 1.5f);
   spinCollider_->SetLayer(CollisionLayer::Player);
   spinCollider_->SetEnabled(false);  // 初期状態では無効

   // キーコンフィグ設定
   keyConfig_ = std::make_unique<GameEngine::KeyConfig>();

   // キーボード設定
   keyConfig_->BindKey("MoveForward", GameEngine::KeyCode::W);
   keyConfig_->BindKey("MoveBackward", GameEngine::KeyCode::S);
   keyConfig_->BindKey("MoveLeft", GameEngine::KeyCode::A);
   keyConfig_->BindKey("MoveRight", GameEngine::KeyCode::D);
   keyConfig_->BindKey("Jump", GameEngine::KeyCode::Space);
   keyConfig_->BindKey("Spin", GameEngine::KeyCode::X);  // Xキーでスピン

   // ゲームパッド設定
   keyConfig_->BindGamePad("Jump", GameEngine::GamePadButton::A);
   keyConfig_->BindGamePad("Spin", GameEngine::GamePadButton::X);  // Xボタンでスピン
   keyConfig_->BindLeftStick("Move");  // 左スティックで移動

   // パラメータ初期化
   moveSpeed_ = 8.0f;
   jumpPower_ = 10.0f;
   turnSpeed_ = 30.0f;
   jumpRequested_ = false;
   hasMovementInput_ = false;

   // 惑星遷移パラメータ初期化
   targetPlanet_ = nullptr;
   isTransitioning_ = false;
   transitionVelocity_ = Vector3(0.0f, 0.0f, 0.0f);
   attractionStrength_ = 20.0f;

   // 初期位置設定
   Vector3 initialPos = model_->GetPosition();
   float initialRadius = initialPos.Length();

   // プレイヤーモデルの原点は中心にあるため、
   // initialRadiusは惑星中心からプレイヤーの中心までの距離
   // モデルの位置が原点の場合、デフォルト値を使用
   if (initialRadius < 0.001f) {
	  initialRadius = 5.5f; // デフォルト値
	  initialPos = Vector3(0.0f, 5.5f, 0.0f);
   }

   // コンポーネント初期化
   gravity_.Initialize(9.8f, playerRadius_);
   sphericalMovement_.Initialize(initialPos, nullptr, initialRadius);

   lastMoveDirection_ = Vector3(0.0f, 0.0f, 1.0f);

   // ステートマシンの初期化
   AttachStateMachine();
   InitializeStateMachine();
   
   // プレイヤー専用ポイントライトの作成
   playerPointLight_ = EngineContext::CreatePointLight(
	  "PlayerPointLight",
	  0xffffffff,                      // 白色
	  initialPos,                       // 初期位置
	  2.5f,                             // 強度
	  5.5f,                            // 半径
	  2.0f                              // 減衰
   );
}

void Player::InitializeStateMachine() {
   if (!stateMachine_) return;

   // 状態を登録
   stateMachine_->AddState("Idle",
	  [this]() { OnEnterIdle(); },
	  [this]() { OnUpdateIdle(); }
   );

   stateMachine_->AddState("Walking",
	  [this]() { OnEnterWalking(); },
	  [this]() { OnUpdateWalking(); }
   );

   stateMachine_->AddState("Jumping",
	  [this]() { OnEnterJumping(); },
	  [this]() { OnUpdateJumping(); }
   );

   stateMachine_->AddState("Falling",
	  [this]() { OnEnterFalling(); },
	  [this]() { OnUpdateFalling(); }
   );

   stateMachine_->AddState("PlanetTransition",
	  [this]() { OnEnterPlanetTransition(); },
	  [this]() { OnUpdatePlanetTransition(); }
   );
   
   stateMachine_->AddState("Spinning",
	  [this]() { OnEnterSpinning(); },
	  [this]() { OnUpdateSpinning(); }
   );

   // 遷移ルールを設定
   stateMachine_->AddTransitionRule("Idle", { "Walking", "Jumping", "Falling", "PlanetTransition", "Spinning" });
   stateMachine_->AddTransitionRule("Walking", { "Idle", "Jumping", "Falling", "PlanetTransition", "Spinning" });
   stateMachine_->AddTransitionRule("Jumping", { "Falling", "Idle", "Walking", "PlanetTransition", "Spinning" });
   stateMachine_->AddTransitionRule("Falling", { "Idle", "Walking", "PlanetTransition", "Spinning" });
   stateMachine_->AddTransitionRule("PlanetTransition", { "Falling", "Idle", "Walking", "Spinning" });
   stateMachine_->AddTransitionRule("Spinning", { "Idle", "Walking", "Falling", "Jumping" });
}

// ========== Idle State ==========
void Player::OnEnterIdle() {
   if (stopCallback_) {
	  stopCallback_();
   }
}

void Player::OnUpdateIdle() {
   CheckMovementInput();
   CheckJumpInput();
   CheckSpinInput();

   if (spinRequested_ && onGround_) {
	  stateMachine_->RequestState("Spinning", 10);
   } else if (jumpRequested_ && onGround_) {
	  stateMachine_->RequestState("Jumping", 10);
   } else if (hasMovementInput_ && onGround_) {
	  stateMachine_->RequestState("Walking", 5);
   } else if (!onGround_) {
	  stateMachine_->RequestState("Falling", 8);
   }
}

// ========== Walking State ==========
void Player::OnEnterWalking() {
   // Walking状態に入った時の処理
}

void Player::OnUpdateWalking() {
   CheckMovementInput();
   CheckJumpInput();
   CheckSpinInput();

   if (hasMovementInput_) {
	  float dt = GameEngine::EngineContext::GetDeltaTime();
	  UpdateMovement(dt);
   }

   if (spinRequested_ && onGround_) {
	  stateMachine_->RequestState("Spinning", 10);
   } else if (jumpRequested_ && onGround_) {
	  stateMachine_->RequestState("Jumping", 10);
   } else if (!hasMovementInput_ && onGround_) {
	  stateMachine_->RequestState("Idle", 5);
   } else if (!onGround_) {
	  stateMachine_->RequestState("Falling", 8);
   }
}

// ========== Jumping State ==========
void Player::OnEnterJumping() {
   gravity_.Jump(jumpPower_);
   onGround_ = false;

   // ジャンプ音を再生
   auto seJump = GameEngine::EngineContext::GetSound("seJump");
   if (seJump) {
	  seJump->Play(0.5f, false);
   }

   if (jumpCallback_) {
	  jumpCallback_(model_->GetPosition());
   }
}

void Player::OnUpdateJumping() {
   CheckMovementInput();
   CheckSpinInput();

   if (hasMovementInput_) {
	  float dt = GameEngine::EngineContext::GetDeltaTime();
	  UpdateMovement(dt);
   }
   
   // ジャンプ中もスピン可能
   if (spinRequested_) {
	  stateMachine_->RequestState("Spinning", 10);
	  return;
   }

   if (gravity_.GetRadialVelocity() <= 0.0f) {
	  stateMachine_->RequestState("Falling", 8);
   }
}

// ========== Falling State ==========
void Player::OnEnterFalling() {
   // Falling状態に入った時の処理
}

void Player::OnUpdateFalling() {
   CheckMovementInput();
   CheckSpinInput();

   if (hasMovementInput_) {
	  float dt = GameEngine::EngineContext::GetDeltaTime();
	  UpdateMovement(dt);
   }
   
   // 落下中もスピン可能
   if (spinRequested_) {
	  stateMachine_->RequestState("Spinning", 10);
	  return;
   }

   if (onGround_) {
	  if (landCallback_) {
		 landCallback_(model_->GetPosition());
	  }

	  if (hasMovementInput_) {
		 stateMachine_->RequestState("Walking", 5);
	  } else {
		 stateMachine_->RequestState("Idle", 5);
	  }
   }
}

// ========== Planet Transition State ==========
void Player::OnEnterPlanetTransition() {
   // 惑星遷移状態に入った時の処理
   isTransitioning_ = true;
   onGround_ = false;

   // 現在の速度を初期化（引力によって加速される）
   transitionVelocity_ = Vector3(0.0f, 0.0f, 0.0f);

   // ターゲット惑星に向けた回転遷移を準備（Slerpで滑らかに回転）
   if (targetPlanet_) {
	  Vector3 currentPos = model_->GetPosition();
	  Vector3 targetPlanetCenter = targetPlanet_->GetWorldPosition();

	  // 現在の完全な回転を保存（position * orientation の合成）
	  transitionStartRotation_ = sphericalMovement_.GetPositionQuaternion() * sphericalMovement_.GetOrientationQuaternion();

	  // 目標となる回転を計算
	  // 1. ターゲット惑星に対する新しいpositionQuaternionを計算
	  Vector3 toTargetPlanet = (targetPlanetCenter - currentPos).Normalize();
	  Vector3 targetHeadDir = toTargetPlanet * -1.0f;

	  Vector3 up = Vector3(0.0f, 1.0f, 0.0f);
	  float dot = up.Dot(targetHeadDir);

	  Quaternion targetPositionQuat;
	  if (std::abs(dot - 1.0f) < 0.001f) {
		 targetPositionQuat = Quaternion::Identity();
	  } else if (std::abs(dot + 1.0f) < 0.001f) {
		 targetPositionQuat = MakeRotateAxisAngleQuaternion(Vector3(1.0f, 0.0f, 0.0f), std::numbers::pi_v<float>);
	  } else {
		 Vector3 axis = up.Cross(targetHeadDir).Normalize();
		 float angle = std::acos(std::clamp(dot, -1.0f, 1.0f));
		 targetPositionQuat = MakeRotateAxisAngleQuaternion(axis, angle);
	  }

	  // 2. 現在の前方向を新しい座標系で維持するorientationQuaternionを計算
	  Vector3 currentForward = sphericalMovement_.GetForwardDirection();

	  Vector3 baseForward = RotateVector(Vector3(0.0f, 0.0f, 1.0f), targetPositionQuat);
	  Vector3 targetTangent = currentForward - targetHeadDir * targetHeadDir.Dot(currentForward);
	  Vector3 baseTangent = baseForward - targetHeadDir * targetHeadDir.Dot(baseForward);

	  Quaternion targetOrientationQuat = Quaternion::Identity();
	  if (targetTangent.Length() > 0.001f && baseTangent.Length() > 0.001f) {
		 targetTangent = targetTangent.Normalize();
		 baseTangent = baseTangent.Normalize();

		 float angleDot = std::clamp(baseTangent.Dot(targetTangent), -1.0f, 1.0f);
		 float orientAngle = std::acos(angleDot);

		 Vector3 cross = baseTangent.Cross(targetTangent);
		 float direction = cross.Dot(targetHeadDir) > 0.0f ? 1.0f : -1.0f;

		 targetOrientationQuat = MakeRotateAxisAngleQuaternion(Vector3(0.0f, 1.0f, 0.0f), orientAngle * direction);
	  }

	  // 3. 目標の完全な回転を計算
	  transitionTargetRotation_ = targetPositionQuat * targetOrientationQuat;
	  transitionTargetRotation_ = transitionTargetRotation_.Normalize();

	  // 4. Quaternionの最短経路を選択（内積が負の場合は符号を反転）
	  float quatDot = transitionStartRotation_.x * transitionTargetRotation_.x +
		 transitionStartRotation_.y * transitionTargetRotation_.y +
		 transitionStartRotation_.z * transitionTargetRotation_.z +
		 transitionStartRotation_.w * transitionTargetRotation_.w;

	  if (quatDot < 0.0f) {
		 // 最短経路のために符号を反転
		 transitionTargetRotation_.x = -transitionTargetRotation_.x;
		 transitionTargetRotation_.y = -transitionTargetRotation_.y;
		 transitionTargetRotation_.z = -transitionTargetRotation_.z;
		 transitionTargetRotation_.w = -transitionTargetRotation_.w;
	  }

	  // 回転遷移の進行度をリセット
	  transitionRotationProgress_ = 0.0f;
   }
}

void Player::OnUpdatePlanetTransition() {
   if (!targetPlanet_) {
	  // ターゲット惑星がない場合は落下状態に遷移
	  stateMachine_->RequestState("Falling", 10);
	  isTransitioning_ = false;
	  return;
   }

   float dt = GameEngine::EngineContext::GetDeltaTime();
   Vector3 currentPos = model_->GetPosition();

   // ターゲット惑星からの引力のみを適用（現在の惑星からの引力は無視）
   transitionVelocity_ = gravity_.ApplyPlanetAttraction(
	  targetPlanet_,
	  currentPos,
	  transitionVelocity_,
	  attractionStrength_,
	  dt
   );

   // 速度に基づいて位置を更新（ワールド座標での移動）
   Vector3 newPos = currentPos + transitionVelocity_ * dt;
   model_->SetPosition(newPos);

   // Slerpを使用した滑らかな回転遷移
   transitionRotationProgress_ += transitionRotationSpeed_ * dt;
   transitionRotationProgress_ = std::clamp(transitionRotationProgress_, 0.0f, 1.0f);

   // Slerpで現在の回転を計算
   Quaternion currentRotation;
   if (transitionRotationProgress_ >= 1.0f) {
	  // 遷移完了
	  currentRotation = transitionTargetRotation_;
   } else {
	  // Slerp補間
	  float t = transitionRotationProgress_;

	  // Quaternionの内積を計算
	  float quatDot = transitionStartRotation_.x * transitionTargetRotation_.x +
		 transitionStartRotation_.y * transitionTargetRotation_.y +
		 transitionStartRotation_.z * transitionTargetRotation_.z +
		 transitionStartRotation_.w * transitionTargetRotation_.w;

	  quatDot = std::clamp(quatDot, -1.0f, 1.0f);

	  // Slerp計算
	  float sinHalfAngle = std::sqrt(1.0f - quatDot * quatDot);

	  if (sinHalfAngle > 0.001f) {
		 // 通常のSlerp
		 float halfAngle = std::acos(quatDot);
		 float ratioA = std::sin((1.0f - t) * halfAngle) / std::sin(halfAngle);
		 float ratioB = std::sin(t * halfAngle) / std::sin(halfAngle);

		 currentRotation.x = transitionStartRotation_.x * ratioA + transitionTargetRotation_.x * ratioB;
		 currentRotation.y = transitionStartRotation_.y * ratioA + transitionTargetRotation_.y * ratioB;
		 currentRotation.z = transitionStartRotation_.z * ratioA + transitionTargetRotation_.z * ratioB;
		 currentRotation.w = transitionStartRotation_.w * ratioA + transitionTargetRotation_.w * ratioB;

		 currentRotation = currentRotation.Normalize();
	  } else {
		 // ほぼ同じ向きの場合は線形補間
		 currentRotation.x = transitionStartRotation_.x * (1.0f - t) + transitionTargetRotation_.x * t;
		 currentRotation.y = transitionStartRotation_.y * (1.0f - t) + transitionTargetRotation_.y * t;
		 currentRotation.z = transitionStartRotation_.z * (1.0f - t) + transitionTargetRotation_.z * t;
		 currentRotation.w = transitionStartRotation_.w * (1.0f - t) + transitionTargetRotation_.w * t;

		 currentRotation = currentRotation.Normalize();
	  }
   }

   // 回転をモデルに直接適用
   model_->SetRotationQuaternion(currentRotation);

   // ターゲット惑星との距離をチェック
   Vector3 targetPlanetCenter = targetPlanet_->GetWorldPosition();
   float distanceToPlanet = (newPos - targetPlanetCenter).Length();
   float targetRadius = targetPlanet_->GetPlanetRadius() + playerRadius_;

   // 惑星の表面に近づいたら、球面移動に切り替え
   const float transitionThreshold = 2.0f;
   if (distanceToPlanet <= targetRadius + transitionThreshold) {
	  // 現在の回転から positionQuaternion と orientationQuaternion を再計算
	  Vector3 toTargetPlanet = (targetPlanetCenter - newPos).Normalize();
	  Vector3 newHeadDir = toTargetPlanet * -1.0f;

	  // 新しいpositionQuaternionを計算
	  Vector3 up = Vector3(0.0f, 1.0f, 0.0f);
	  float dot = up.Dot(newHeadDir);

	  Quaternion newPositionQuat;
	  if (std::abs(dot - 1.0f) < 0.001f) {
		 newPositionQuat = Quaternion::Identity();
	  } else if (std::abs(dot + 1.0f) < 0.001f) {
		 newPositionQuat = MakeRotateAxisAngleQuaternion(Vector3(1.0f, 0.0f, 0.0f), std::numbers::pi_v<float>);
	  } else {
		 Vector3 axis = up.Cross(newHeadDir).Normalize();
		 float angle = std::acos(std::clamp(dot, -1.0f, 1.0f));
		 newPositionQuat = MakeRotateAxisAngleQuaternion(axis, angle);
	  }

	  sphericalMovement_.SetPositionQuaternion(newPositionQuat);

	  // 現在の前方向を維持するようにorientationQuaternionを計算
	  // currentRotation = newPositionQuat * newOrientationQuat となるように
	  // newOrientationQuat = newPositionQuat.Conjugate() * currentRotation
	  Quaternion newOrientationQuat = newPositionQuat.Conjugate() * currentRotation;
	  newOrientationQuat = newOrientationQuat.Normalize();
	  sphericalMovement_.SetOrientationQuaternion(newOrientationQuat);

	  // 新しい惑星に切り替え
	  float newDistance = (newPos - targetPlanetCenter).Length();
	  sphericalMovement_.SetCurrentRadius(newDistance);
	  sphericalMovement_.SetCurrentPlanet(targetPlanet_);

	  // 遷移中の速度を半径方向の速度に変換（着地の勢いとして保持）
	  Vector3 toPlanetDir = (targetPlanetCenter - newPos).Normalize();
	  float radialSpeed = transitionVelocity_.Dot(toPlanetDir);
	  gravity_.SetRadialVelocity(-radialSpeed * 0.5f); // 着地時の勢いを少し残す

	  // 遷移完了
	  isTransitioning_ = false;
	  targetPlanet_ = nullptr;
	  transitionVelocity_ = Vector3(0.0f, 0.0f, 0.0f);

	  // 落下状態に遷移
	  stateMachine_->RequestState("Falling", 10);
   }

   // 入力移動も可能にする（空中制御）
   CheckMovementInput();
   if (hasMovementInput_) {
	  // 移動入力があれば若干の空中制御を許可
	  Vector3 headDir = (newPos - targetPlanetCenter).Normalize();
	  Vector3 moveDir = CameraInputHelper::CalculateTangentMoveDirection(
		 moveDirection_.x, moveDirection_.z, headDir, cameraController_
	  );

	  if (moveDir.Length() > 0.001f) {
		 // 接平面方向への空中制御力を追加
		 float airControlStrength = moveSpeed_ * 0.3f; // 通常の30%の制御力
		 transitionVelocity_ = transitionVelocity_ + moveDir * airControlStrength * dt;
	  }
   }
}

// ========== Spinning State ==========
void Player::OnEnterSpinning() {
   isSpinning_ = true;
   spinTimer_ = 0.0f;
   spinRotationAngle_ = 0.0f;
   
   // スピン用コライダーを有効化
   if (spinCollider_) {
	  spinCollider_->SetEnabled(true);
   }
   
   // スピン音を再生（ジャンプ音を流用）
   auto seSpin = GameEngine::EngineContext::GetSound("seSpin");
   if (seSpin) {
	  seSpin->Play(0.5f, false);
   }
}

void Player::OnUpdateSpinning() {
   float dt = GameEngine::EngineContext::GetDeltaTime();
   spinTimer_ += dt;
   
   // スピン中も移動可能
   CheckMovementInput();
   if (hasMovementInput_) {
	  UpdateMovement(dt);
   }
   
   // スピン中もジャンプ可能
   CheckJumpInput();
   if (jumpRequested_ && onGround_) {
	  // スピンをキャンセルしてジャンプ
	  isSpinning_ = false;
	  spinTimer_ = 0.0f;
	  spinRotationAngle_ = 0.0f;
	  
	  if (spinCollider_) {
		 spinCollider_->SetEnabled(false);
	  }
	  
	  stateMachine_->RequestState("Jumping", 10);
	  return;
   }
   
   // 0.5秒間で360度回転（2π radians）
   float targetAngle = std::numbers::pi_v<float> * 2.0f;  // 360度
   float progress = spinTimer_ / spinDuration_;
   
   if (progress < 1.0f) {
	  // 回転中
	  spinRotationAngle_ = targetAngle * progress;
   } else {
	  // スピン終了
	  isSpinning_ = false;
	  spinTimer_ = 0.0f;
	  spinRotationAngle_ = 0.0f;
	  
	  // スピン用コライダーを無効化
	  if (spinCollider_) {
		 spinCollider_->SetEnabled(false);
	  }
	  
	  // 地面にいる場合
	  if (onGround_) {
		 CheckMovementInput();
		 if (hasMovementInput_) {
			stateMachine_->RequestState("Walking", 5);
		 } else {
			stateMachine_->RequestState("Idle", 5);
		 }
	  } else {
		 stateMachine_->RequestState("Falling", 8);
	  }
   }
}

// ========== Common Methods ==========
void Player::CheckMovementInput() {
   float inputX = 0.0f;
   float inputZ = 0.0f;

   // キーボード入力
   if (keyConfig_->IsPressed("MoveRight")) inputX -= 1.0f;
   if (keyConfig_->IsPressed("MoveLeft"))  inputX += 1.0f;
   if (keyConfig_->IsPressed("MoveForward"))  inputZ += 1.0f;
   if (keyConfig_->IsPressed("MoveBackward")) inputZ -= 1.0f;

   // ゲームパッドのスティック入力（キーボード入力が無い場合のみ）
   if (inputX == 0.0f && inputZ == 0.0f) {
	  Vector2 stickInput = keyConfig_->GetStickVector("Move", 0, 0.24f);
	  if (stickInput.x != 0.0f || stickInput.y != 0.0f) {
		 inputX = -stickInput.x;  // X軸を反転（右が負、左が正）
		 inputZ = stickInput.y;
	  }
   }

   hasMovementInput_ = (inputX != 0.0f || inputZ != 0.0f);
   moveDirection_ = Vector3(inputX, 0.0f, inputZ);
}

void Player::CheckJumpInput() {
   jumpRequested_ = keyConfig_->IsTriggered("Jump");
}

void Player::CheckSpinInput() {
   spinRequested_ = keyConfig_->IsTriggered("Spin");
}

void Player::Update(float dt) {
   if (!model_) return;

   if (planetSwitchCooldown_ > 0.0f) {
	  planetSwitchCooldown_ -= dt;
   }

   // 1. 重力処理（遷移中も適用）
   bool isInAir = false;
   if (stateMachine_) {
	  const std::string& currentState = stateMachine_->GetCurrentState();
	  isInAir = (currentState == "Jumping" || currentState == "Falling" || currentState == "PlanetTransition");
   }

   // 遷移中でない場合は通常の重力計算
   if (!isTransitioning_) {
	  Planet* currentPlanet = sphericalMovement_.GetCurrentPlanet();
	  float newRadius = gravity_.UpdateGravity(
		 currentPlanet,
		 model_->GetPosition(),
		 sphericalMovement_.GetCurrentRadius(),
		 dt,
		 isInAir
	  );
	  sphericalMovement_.SetCurrentRadius(newRadius);
	  onGround_ = gravity_.IsOnGround();

	  // 惑星の重力範囲外にいる場合、最も近い惑星を探して引き寄せる
	  // ただし、現在の惑星がある場合はその惑星のコリジョンリストから外れた場合のみ
	  if (isInAir && currentPlanet) {
		 // 現在の惑星がコリジョンリストにあるかチェック
		 bool isCurrentPlanetInCollision = false;
		 for (Planet* planet : collidingPlanets_) {
			if (planet == currentPlanet) {
			   isCurrentPlanetInCollision = true;
			   break;
			}
		 }

		 // 現在の惑星から完全に離れた場合のみ、別の惑星への遷移を試みる
		 if (!isCurrentPlanetInCollision && !collidingPlanets_.empty()) {
			// コリジョン中の惑星の中で最も近い惑星を探す
			Planet* nearestPlanet = FindNearestPlanetInRange(500.0f);

			if (nearestPlanet && nearestPlanet != currentPlanet) {
			   // 新しい惑星への遷移を開始
			   InitiatePlanetSwitch(nearestPlanet);
			}
		 }
	  }
   }

   // 2. ステートマシンの更新（状態遷移 + 状態更新）
   if (stateMachine_) {
	  stateMachine_->Update();
   }

   // 3. 位置と回転の適用（惑星遷移中以外）
   if (!isTransitioning_) {
	  Planet* currentPlanet = sphericalMovement_.GetCurrentPlanet();
	  if (currentPlanet) {
		 // 惑星が移動している場合、プレイヤーの位置を惑星の動きに合わせて更新
		 sphericalMovement_.UpdateModelPosition(model_);
		 
		 // 通常の回転を適用
		 sphericalMovement_.ApplyRotationToModel(model_);
		 
		 // スピン中は追加でY軸回転を適用
		 if (isSpinning_) {
			// 現在のモデルの回転を取得
			GameEngine::Quaternion currentRotation = model_->GetRotationQuaternion();
			
			// ローカルY軸回転のQuaternionを作成
			GameEngine::Quaternion spinRotation = MakeRotateAxisAngleQuaternion(Vector3(0.0f, 1.0f, 0.0f), spinRotationAngle_);
			
			// 現在の回転にスピン回転を乗算（ローカル空間での回転）
			GameEngine::Quaternion finalRotation = currentRotation * spinRotation;
			
			// モデルに回転を適用
			model_->SetRotationQuaternion(finalRotation);
		 }

		 // 惑星の動きをプレイヤーに適用（着地している場合のみ）
		 if (onGround_) {
			Vector3 planetMovement = currentPlanet->GetFrameMovement();
			if (planetMovement.Length() > 0.001f) {
			   Vector3 currentPos = model_->GetPosition();
			   model_->SetPosition(currentPos + planetMovement);
			}
		 }
	  }
   }

   // 4. プレイヤーライトの更新
   UpdatePlayerLight();

#ifdef USE_IMGUI
   ShowDebugInfo();
   DrawGreatCircles();
#endif
}

Planet* Player::FindNearestPlanetInRange(float maxDistance) {
   // コリジョン中の惑星の中から最も近い惑星を探す
   if (collidingPlanets_.empty() || !model_) {
	  return nullptr;
   }

   Vector3 currentPos = model_->GetPosition();
   Planet* nearestPlanet = nullptr;
   float minDistance = maxDistance;

   for (Planet* planet : collidingPlanets_) {
	  if (!planet) continue;

	  Vector3 planetCenter = planet->GetWorldPosition();
	  float distance = (planetCenter - currentPos).Length();

	  // コリジョン中の惑星の中で最も近い惑星を探す
	  if (distance < minDistance) {
		 minDistance = distance;
		 nearestPlanet = planet;
	  }
   }

   return nearestPlanet;
}

bool Player::IsInsideAnyGravityField() {
   // コリジョン中の惑星が1つでもあれば、重力範囲内と判定
   return !collidingPlanets_.empty();
}

void Player::UpdateMovement(float dt) {
   Planet* currentPlanet = sphericalMovement_.GetCurrentPlanet();
   if (!currentPlanet) return;

   // 入力取得
   float inputX = 0.0f;
   float inputZ = 0.0f;

   // キーボード入力
   if (keyConfig_->IsPressed("MoveRight")) inputX -= 1.0f;
   if (keyConfig_->IsPressed("MoveLeft"))  inputX += 1.0f;
   if (keyConfig_->IsPressed("MoveForward"))  inputZ += 1.0f;
   if (keyConfig_->IsPressed("MoveBackward")) inputZ -= 1.0f;

   // ゲームパッドのスティック入力（キーボード入力が無い場合のみ）
   if (inputX == 0.0f && inputZ == 0.0f) {
	  Vector2 stickInput = keyConfig_->GetStickVector("Move", 0, 0.24f);
	  if (stickInput.x != 0.0f || stickInput.y != 0.0f) {
		 inputX = -stickInput.x;  // X軸を反転（右が負、左が正）
		 inputZ = stickInput.y;
	  }
   }

   if (inputX == 0.0f && inputZ == 0.0f) {
	  moveDirection_ = Vector3(0.0f, 0.0f, 0.0f);

	  if (stopCallback_) {
		 stopCallback_();
	  }

	  return;
   }

   // カメラに基づいた移動方向を計算
   Vector3 headDir = sphericalMovement_.GetHeadDirection();
   Vector3 moveDir = CameraInputHelper::CalculateTangentMoveDirection(
	  inputX, inputZ, headDir, cameraController_
   );

   if (moveDir.Length() < 0.001f) {
	  return;
   }

   // 最後の移動方向を記録（回転用）
   lastMoveDirection_ = moveDir;

   // 移動
   float moveDistance = moveSpeed_ * dt;
   sphericalMovement_.Move(moveDir, moveDistance);

   // 向きを更新
   sphericalMovement_.UpdateOrientation(lastMoveDirection_, turnSpeed_, dt);

   // 移動中のコールバックを呼び出す
   if (moveCallback_) {
	  Vector3 currentPos = model_->GetPosition();
	  moveCallback_(currentPos);

	  // ジャンプ中や落下中は停止コールバックを呼ぶ
	  if (stateMachine_) {
		 const std::string& currentState = stateMachine_->GetCurrentState();
		 if (currentState == "Jumping" || currentState == "Falling") {
			if (stopCallback_) {
			   stopCallback_();
			}
		 }
	  }
   }

   moveDirection_ = Vector3(inputX, 0.0f, inputZ);
}

void Player::InitiatePlanetSwitch(Planet* newPlanet) {
   if (!newPlanet || isTransitioning_) {
	  return;
   }

   // ターゲット惑星を設定
   targetPlanet_ = newPlanet;

   // 惑星遷移状態に遷移
   if (stateMachine_) {
	  stateMachine_->RequestState("PlanetTransition", 100);
   }
}

void Player::OnCollisionEnter(GameObject* other) {
   // ラビットとの衝突判定
   Rabbit* rabbit = dynamic_cast<Rabbit*>(other);
   if (rabbit) {
	  if (rabbitCaptureCallback_) {
		 rabbitCaptureCallback_(rabbit);
	  }
	  return;
   }

   Planet* planet = dynamic_cast<Planet*>(other);
   if (planet) {
	  // コリジョン中の惑星リストに追加
	  auto it = std::find(collidingPlanets_.begin(), collidingPlanets_.end(), planet);
	  if (it == collidingPlanets_.end()) {
		 collidingPlanets_.push_back(planet);
	  }

	  Planet* currentPlanet = sphericalMovement_.GetCurrentPlanet();

	  if (!currentPlanet) {
		 sphericalMovement_.SetCurrentPlanet(planet);
		 Vector3 playerPos = model_->GetPosition();
		 sphericalMovement_.Initialize(playerPos, planet, (playerPos - planet->GetWorldPosition()).Length());
		 return;
	  }

	  if (planetSwitchCooldown_ > 0.0f || isTransitioning_) {
		 return;
	  }

	  Vector3 playerPos = model_->GetPosition();

	  // 惑星切り替え判定
	  if (PlanetSwitchHelper::ShouldSwitchPlanet(currentPlanet, planet, playerPos, 0.5f)) {
		 planetSwitchCooldown_ = 0.3f;

		 // 引力による惑星遷移を開始
		 InitiatePlanetSwitch(planet);
	  }
   }
}

void Player::OnCollisionStay(GameObject* other) {
   (void)other;
   Planet* planet = dynamic_cast<Planet*>(other);
   if (planet) {
	  // コリジョン中の惑星リストに追加（Stay時も念のため確認）
	  auto it = std::find(collidingPlanets_.begin(), collidingPlanets_.end(), planet);
	  if (it == collidingPlanets_.end()) {
		 collidingPlanets_.push_back(planet);
	  }

	  Planet* currentPlanet = sphericalMovement_.GetCurrentPlanet();

	  if (!currentPlanet) {
		 sphericalMovement_.SetCurrentPlanet(planet);
		 Vector3 playerPos = model_->GetPosition();
		 sphericalMovement_.Initialize(playerPos, planet, (playerPos - planet->GetWorldPosition()).Length());
		 return;
	  }

	  if (planetSwitchCooldown_ > 0.0f || isTransitioning_) {
		 return;
	  }

	  if (planet == currentPlanet) {
		 return;
	  }

	  Vector3 playerPos = model_->GetPosition();

	  // 惑星切り替え判定
	  if (PlanetSwitchHelper::ShouldSwitchPlanet(currentPlanet, planet, playerPos, 0.2f)) {
		 planetSwitchCooldown_ = 0.3f;

		 // 引力による惑星遷移を開始
		 InitiatePlanetSwitch(planet);
	  }
   }
}

void Player::OnCollisionExit(GameObject* other) {
   Planet* planet = dynamic_cast<Planet*>(other);
   if (planet) {
	  // コリジョン中の惑星リストから削除
	  auto it = std::find(collidingPlanets_.begin(), collidingPlanets_.end(), planet);
	  if (it != collidingPlanets_.end()) {
		 collidingPlanets_.erase(it);
	  }
   }
}

void Player::ShowDebugInfo() {
#ifdef USE_IMGUI
   ImGui::Begin("Player Debug");
   if (stateMachine_) {
	  ImGui::Text("Current State: %s", stateMachine_->GetCurrentState().c_str());
   }
   ImGui::Text("On Ground: %s", onGround_ ? "true" : "false");
   ImGui::Text("Has Movement: %s", hasMovementInput_ ? "true" : "false");
   ImGui::Text("Jump Requested: %s", jumpRequested_ ? "true" : "false");
   ImGui::Text("Radial Velocity: %.2f", gravity_.GetRadialVelocity());
   ImGui::Text("Is Transitioning: %s", isTransitioning_ ? "true" : "false");

   // コリジョン中の惑星情報
   ImGui::Separator();
   ImGui::Text("=== Collision Info ===");
   ImGui::Text("Colliding Planets: %d", static_cast<int>(collidingPlanets_.size()));
   Planet* currentPlanet = sphericalMovement_.GetCurrentPlanet();
   if (currentPlanet) {
	  bool isCurrentInCollision = false;
	  for (Planet* planet : collidingPlanets_) {
		 if (planet == currentPlanet) {
			isCurrentInCollision = true;
			break;
		 }
	  }
	  ImGui::Text("Current Planet in Collision: %s", isCurrentInCollision ? "YES" : "NO");
   }

   if (isTransitioning_) {
	  ImGui::Separator();
	  ImGui::Text("=== Transition Info ===");
	  ImGui::Text("Transition Velocity: (%.2f, %.2f, %.2f)",
		 transitionVelocity_.x, transitionVelocity_.y, transitionVelocity_.z);
	  ImGui::Text("Speed: %.2f", transitionVelocity_.Length());

	  // 回転遷移の情報
	  ImGui::Text("Rotation Progress: %.2f%%", transitionRotationProgress_ * 100.0f);
	  ImGui::Text("Start Rotation: (%.3f, %.3f, %.3f, %.3f)",
		 transitionStartRotation_.x, transitionStartRotation_.y,
		 transitionStartRotation_.z, transitionStartRotation_.w);
	  ImGui::Text("Target Rotation: (%.3f, %.3f, %.3f, %.3f)",
		 transitionTargetRotation_.x, transitionTargetRotation_.y,
		 transitionTargetRotation_.z, transitionTargetRotation_.w);

	  // 現在の回転
	  Quaternion currentRot = model_->GetRotationQuaternion();
	  ImGui::Text("Current Rotation: (%.3f, %.3f, %.3f, %.3f)",
		 currentRot.x, currentRot.y, currentRot.z, currentRot.w);

	  // 回転角度の差
	  float quatDot = transitionStartRotation_.x * transitionTargetRotation_.x +
		 transitionStartRotation_.y * transitionTargetRotation_.y +
		 transitionStartRotation_.z * transitionTargetRotation_.z +
		 transitionStartRotation_.w * transitionTargetRotation_.w;
	  float rotationAngle = std::acos(std::clamp(std::abs(quatDot), 0.0f, 1.0f)) * 2.0f;
	  ImGui::Text("Rotation Angle: %.2f degrees", rotationAngle * 180.0f / std::numbers::pi_v<float>);

	  if (targetPlanet_) {
		 Vector3 playerPos = model_->GetPosition();
		 Vector3 planetPos = targetPlanet_->GetWorldPosition();
		 float distance = (planetPos - playerPos).Length();
		 float surfaceDistance = distance - targetPlanet_->GetPlanetRadius();
		 ImGui::Text("Distance to Target Planet: %.2f", distance);
		 ImGui::Text("Distance from Surface: %.2f", surfaceDistance);

		 // 引力の方向を可視化
		 Vector3 attractionDir = (planetPos - playerPos).Normalize();
		 ImGui::Text("Attraction Direction: (%.2f, %.2f, %.2f)",
			attractionDir.x, attractionDir.y, attractionDir.z);
	  }

	  if (currentPlanet && targetPlanet_ && currentPlanet != targetPlanet_) {
		 Vector3 playerPos = model_->GetPosition();
		 Vector3 currentPlanetPos = currentPlanet->GetWorldPosition();
		 float distanceToCurrent = (currentPlanetPos - playerPos).Length();
		 ImGui::Text("Distance to Current Planet: %.2f", distanceToCurrent);
	  }
   }

   ImGui::Separator();
   // 引力の強さと回転速度を調整できるスライダー
   ImGui::SliderFloat("Attraction Strength", &attractionStrength_, 1.0f, 100.0f);
   ImGui::SliderFloat("Rotation Speed", &transitionRotationSpeed_, 0.5f, 10.0f);

   ImGui::End();
#endif
}

Vector3 Player::GetForward() const {
   return sphericalMovement_.GetForwardDirection();
}

Quaternion Player::GetRotationQuaternion() const {
   return sphericalMovement_.GetPositionQuaternion();
}

void Player::SetCurrentPlanet(Planet* planet) {
   sphericalMovement_.SetCurrentPlanet(planet);
}

Planet* Player::GetCurrentPlanet() const {
   return sphericalMovement_.GetCurrentPlanet();
}

void Player::DrawGreatCircles() {
#ifdef USE_IMGUI
   Planet* currentPlanet = sphericalMovement_.GetCurrentPlanet();

   // 遷移中の場合は引力の可視化
   if (isTransitioning_ && targetPlanet_) {
	  Vector3 playerPos = model_->GetPosition();
	  Vector3 targetPlanetPos = targetPlanet_->GetWorldPosition();

	  // ターゲット惑星への引力線（マゼンタ）
	  Vector4 attractionColor = Vector4(1.0f, 0.0f, 1.0f, 1.0f);
	  GameEngine::EngineContext::DrawLine(playerPos, targetPlanetPos, attractionColor);

	  // 速度ベクトルの可視化（シアン）
	  if (transitionVelocity_.Length() > 0.001f) {
		 Vector3 velocityEnd = playerPos + transitionVelocity_.Normalize() * 3.0f;
		 Vector4 velocityColor = Vector4(0.0f, 1.0f, 1.0f);
		 GameEngine::EngineContext::DrawLine(playerPos, velocityEnd, velocityColor);
	  }

	  // 現在の惑星への引力線（薄い黄色）
	  if (currentPlanet && currentPlanet != targetPlanet_) {
		 Vector3 currentPlanetPos = currentPlanet->GetWorldPosition();
		 Vector4 currentAttractionColor = Vector4(1.0f, 1.0f, 0.5f, 0.5f);
		 GameEngine::EngineContext::DrawLine(playerPos, currentPlanetPos, currentAttractionColor);
	  }

	  return; // 遷移中は通常のデバッグ表示はスキップ
   }

   if (!currentPlanet) return;

   Vector3 planetCenter = currentPlanet->GetWorldPosition();
   Vector3 playerPos = model_->GetPosition();
   float radius = sphericalMovement_.GetCurrentRadius();

   // プレイヤー位置の法線（頭の方向）
   Vector3 N = sphericalMovement_.GetHeadDirection();

   Vector3 WorldY = Vector3(0, 1, 0);

   // 経度線の回転軸
   Vector3 longitudeAxis = N.Cross(WorldY).Normalize();
   if (longitudeAxis.Length() < 0.0001f) {
	  longitudeAxis = Vector3(1, 0, 0);
   }

   // 緯度線の回転軸
   Vector3 latitudeAxis = N.Cross(longitudeAxis).Normalize();

   // プレイヤー位置マーカー
   Vector4 pointColor = Vector4(1.0f, 1.0f, 0.0f, 1.0f);

   float markerSize = 0.5f;
   GameEngine::EngineContext::DrawLine(
	  playerPos + Vector3(markerSize, 0, 0),
	  playerPos - Vector3(markerSize, 0, 0),
	  pointColor
   );
   GameEngine::EngineContext::DrawLine(
	  playerPos + Vector3(0, markerSize, 0),
	  playerPos - Vector3(0, markerSize, 0),
	  pointColor
   );
   GameEngine::EngineContext::DrawLine(
	  playerPos + Vector3(0, 0, markerSize),
	  playerPos - Vector3(0, 0, markerSize),
	  pointColor
   );

   // 回転軸を可視化
   Vector4 longitudeAxisColor = Vector4(0.0f, 1.0f, 0.0f, 0.5f);
   GameEngine::EngineContext::DrawLine(
	  planetCenter - longitudeAxis * (radius * 0.3f),
	  planetCenter + longitudeAxis * (radius * 0.3f),
	  longitudeAxisColor
   );

   Vector4 latitudeAxisColor = Vector4(1.0f, 0.0f, 1.0f, 0.5f);
   GameEngine::EngineContext::DrawLine(
	  planetCenter - latitudeAxis * (radius * 0.3f),
	  planetCenter + latitudeAxis * (radius * 0.3f),
	  latitudeAxisColor
   );

   // 前方向ベクトルを可視化
   Vector3 forward = sphericalMovement_.GetForwardDirection();
   Vector4 forwardColor = Vector4(0.0f, 1.0f, 1.0f, 1.0f);
   GameEngine::EngineContext::DrawLine(
	  playerPos,
	  playerPos + forward * 2.0f,
	  forwardColor
   );
#endif
}

void Player::UpdatePlayerLight() {
   // ポイントライトのデータを更新
   if (playerPointLight_ && model_) {
	  auto* pointLightData = playerPointLight_->GetPointLightData();
	  if (pointLightData) {
		 pointLightData->position = model_->GetPosition();
	  }
   }
}
