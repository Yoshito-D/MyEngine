// Object3DのVSからPSへ渡す補間契約。
// 照明用の法線と位置はワールド空間、テクスチャ投影用はローカル空間で渡す。
struct VertexShaderOutput {
   float32_t4 position : SV_POSITION;    // ラスタライザーへ渡すクリップ座標
   float32_t2 texCoord : TEXCOORD0;      // マテリアルUV変換前のメッシュUV
   float32_t3 normal : NORMAL0;          // 逆転置行列で変換したワールド法線
   float32_t3 worldPosition : POSITION0; // 点・スポット・エリアライトおよび視線計算用のワールド位置
   float32_t3 localPosition : TEXCOORD1; // 移動・回転に追従するテクスチャ投影用の位置
   float32_t3 localNormal : TEXCOORD2;   // ローカル空間の投影面を選ぶ法線
};

// 数値はCPU側 Material::LightingMode と共有するGPU ABIであり、列挙順を独立に変更してはならない。
#define LIGHTING_NONE 0
#define LIGHTING_LAMBERT 1
#define LIGHTING_HALF_LAMBERT 2
#define LIGHTING_PHONG 3
#define LIGHTING_BLINN_PHONG 4

// ConstantBuffer<Material>はCPU側Material::MaterialData（144バイト）と同じ順序で配置する。
// lightingModeとenvironmentCoefficientの後ろは定数バッファ規則で16バイト境界まで暗黙に空き、
// CPU側のpadding[2]と対応する。行列はDXCの-Zpr指定によりrow-majorのまま転送される。
struct Material {
   float32_t4 color;                 ///< 表示色のRGBA成分。
   int32_t lightingMode;             ///< 照明計算の方式。NONE・Lambert・Phongなどの列挙値をCPUと共有する。
   float32_t environmentCoefficient; ///< 環境マップの強度。
   float32_t4x4 uvTransform;         ///< UV変換行列。
   float32_t shininess;              ///< 光沢度。
   float32_t rimLightIntensity;      ///< リムライトの強度。
   float32_t rimLightPower;          ///< リムライトの減衰指数。
   float32_t fillLightIntensity;     ///< フィルライトの強度。
   float32_t4 rimLightColor;         ///< リムライトの色。
   float32_t4 fillLightColor;        ///< フィルライトの色。
};

// 以下のライトはStructuredBufferとして読み出すため、フィールド順と明示paddingが
// CPU側LightDataBufferの各Data構造体およびSRVのStructureByteStrideと一致している必要がある。
struct DirectionalLight {
   float32_t4 color;     ///< 表示色のRGBA成分。
   float32_t3 direction; ///< 光が進むワールド方向。
   float intensity;      ///< 効果または光の強さ。
};

// 48バイトstride。末尾paddingは次要素の先頭をCPUレイアウトと一致させる。
struct PointLight {
   float32_t4 color;    ///< 表示色のRGBA成分。
   float32_t3 position; ///< 光源中心のワールド位置。
   float intensity;     ///< 効果または光の強さ。
   float radius;        ///< 形状または効果範囲の半径。
   float decay;         ///< 距離減衰。
   float padding[2];    ///< GPU転送データの配置境界を揃える未使用領域。
};

// 64バイトstride。
struct SpotLight {
   float32_t4 color;          ///< 表示色のRGBA成分。
   float32_t3 position;       ///< 光源中心のワールド位置。
   float32_t intensity;       ///< 効果または光の強さ。
   float32_t3 direction;      ///< 光が進むワールド方向。
   float32_t distance;        ///< 基準位置からの距離。
   float32_t decay;           ///< 距離減衰。
   float32_t cosAngle;        ///< スポット光の外側コーン角の余弦。
   float32_t cosFalloffStart; ///< スポット光の減衰開始コーン角の余弦。
   float padding;             ///< GPU転送データの配置境界を揃える未使用領域。
};

// 80バイトstride。
struct AreaLight {
   float32_t4 color;    ///< 表示色のRGBA成分。
   float32_t3 position; ///< 光源中心のワールド位置。
   float32_t intensity; ///< 効果または光の強さ。
   float32_t3 normal;   ///< 面の向きを表す法線ベクトル。
   float32_t width;     ///< 対象領域の横幅。
   float32_t3 tangent;  ///< ライトの右方向ベクトル。
   float32_t height;    ///< 対象領域の高さ。
   float32_t3 padding1; ///< GPU転送データの配置境界を揃える未使用領域。
   float padding2;      ///< GPU転送データの配置境界を揃える未使用領域。
};

// b2の1レジスター（16バイト）に収まり、各StructuredBufferを走査する有効要素数を示す。
struct LightCount {
   uint directionalLightCount; ///< GPUへ渡す有効な平行光源の数。
   uint pointLightCount;       ///< GPUへ渡す有効な点光源の数。
   uint spotLightCount;        ///< GPUへ渡す有効なスポット光源の数。
   uint areaLightCount;        ///< GPUへ渡す有効な矩形光源の数。
};
