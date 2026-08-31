// Object3DのVSからPSへ渡す補間契約。
// 法線と位置はPSの各種ライト計算・視線計算がそのまま利用するため、どちらもワールド空間で渡す。
struct VertexShaderOutput {
    float32_t4 position : SV_POSITION;  // ラスタライザーへ渡すクリップ座標
    float32_t2 texCoord : TEXCOORD0;    // マテリアルUV変換前のメッシュUV
    float32_t3 normal : NORMAL0;        // 逆転置行列で変換したワールド法線
    float32_t3 worldPosition : POSITION0; // 点・スポット・エリアライトおよび視線計算用のワールド位置
};

// 数値はCPU側 Material::LightingMode と共有するGPU ABIであり、列挙順を独立に変更してはならない。
#define LIGHTING_NONE          0
#define LIGHTING_LAMBERT       1
#define LIGHTING_HALF_LAMBERT  2
#define LIGHTING_PHONG         3  
#define LIGHTING_BLINN_PHONG   4  

// ConstantBuffer<Material>はCPU側Material::MaterialData（144バイト）と同じ順序で配置する。
// lightingModeとenvironmentCoefficientの後ろは定数バッファ規則で16バイト境界まで暗黙に空き、
// CPU側のpadding[2]と対応する。行列はDXCの-Zpr指定によりrow-majorのまま転送される。
struct Material
{
    float32_t4 color;
    int32_t lightingMode;
    float32_t environmentCoefficient;
    float32_t4x4 uvTransform;
    float32_t shininess;
    float32_t rimLightIntensity;
    float32_t rimLightPower;
    float32_t fillLightIntensity;
    float32_t4 rimLightColor;
    float32_t4 fillLightColor;
};

// 以下のライトはStructuredBufferとして読み出すため、フィールド順と明示paddingが
// CPU側LightDataBufferの各Data構造体およびSRVのStructureByteStrideと一致している必要がある。
struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float intensity;
};

// 48バイトstride。末尾paddingは次要素の先頭をCPUレイアウトと一致させる。
struct PointLight
{
    float32_t4 color;
    float32_t3 position;
    float intensity;
    float radius;
    float decay;
    float padding[2];
};

// 64バイトstride。
struct SpotLight
{
    float32_t4 color;
    float32_t3 position;
    float32_t intensity;
    float32_t3 direction;
    float32_t distance;
    float32_t decay;
    float32_t cosAngle;
    float32_t cosFalloffStart;
    float padding;
};

// 80バイトstride。
struct AreaLight
{
    float32_t4 color;
    float32_t3 position;
    float32_t intensity;
    float32_t3 normal;
    float32_t width;
    float32_t3 tangent;
    float32_t height;
    float32_t3 padding1;
    float padding2;
};

// b2の1レジスター（16バイト）に収まり、各StructuredBufferを走査する有効要素数を示す。
struct LightCount
{
    uint directionalLightCount;
    uint pointLightCount;
    uint spotLightCount;
    uint areaLightCount;
};
