struct VertexShaderOutput {
    float32_t4 position : SV_POSITION;
    float32_t2 texCoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t3 worldPosition : POSITION0;
};

#define LIGHTING_NONE          0
#define LIGHTING_LAMBERT       1
#define LIGHTING_HALF_LAMBERT  2
#define LIGHTING_PHONG         3  
#define LIGHTING_BLINN_PHONG   4  

struct Material
{
    float32_t4 color;
    int32_t lightingMode;
    float32_t4x4 uvTransform;
    float32_t shininess;
};

struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float intensity;
};

struct PointLight
{
    float32_t4 color;
    float32_t3 position;
    float intensity;
    float radius;
    float decay;
    float padding[2];
};

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

struct LightCount
{
    uint directionalLightCount;
    uint pointLightCount;
    uint spotLightCount;
    uint areaLightCount;
};