#pragma once

// 可読性を高めるための衝突レイヤー定義
enum class CollisionLayer {
   Default = 0,
   Player,
   Enemy,
   Planet,
   EnemyBullet,
   Star,
   Box,  // 箱オブジェクト
   Count
};
