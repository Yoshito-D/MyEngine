float3 CalculateLambert(float3 normal, float3 lightDirection)
{
    float cos = dot(normal, -lightDirection);
    return float3(1.0f, 1.0f, 1.0f) * max(cos, 0.0f);
}

float3 CalculateHalfLambert(float3 normal, float3 lightDirection)
{
    float cos = dot(normal, -lightDirection);
    float halfLambert = cos * 0.5f + 0.5f;
    return float3(1.0f, 1.0f, 1.0f) * pow(halfLambert, 2.0f);
}

float3 CalculatePhongSpecular(float3 normal, float3 lightDirection, float3 toEye, float shininess)
{
    float3 reflectLight = reflect(lightDirection, normal);
    float rDotE = dot(reflectLight, toEye);
    float specularPow = pow(saturate(rDotE), shininess);
    return float3(1.0f, 1.0f, 1.0f) * specularPow;
}

float3 CalculateBlinnPhongSpecular(float3 normal, float3 lightDirection, float3 toEye, float shininess)
{
    float3 halfVector = normalize(-lightDirection + toEye);
    float nDotH = dot(normal, halfVector);
    float specularPow = pow(saturate(nDotH), shininess);
    return float3(1.0f, 1.0f, 1.0f) * specularPow;
}

float CalculatePointLightAttenuation(float distance, float radius, float decay)
{
    return pow(saturate(-distance / radius + 1.0f), decay);
}

float CalculateSpotLightAttenuation(float distance, float lightDistance, float decay){
    return pow(saturate(-distance / lightDistance + 1.0f), decay);
}

float3 CalculateAreaLightContribution(float3 surfacePos, float3 surfaceNormal, float3 lightPos, 
                                     float3 lightNormal, float3 lightTangent, float width, float height,
                                     float3 toEye, float shininess, 
                                     int lightingMode, out float3 specular)
{
    float3 bitangent = normalize(cross(lightNormal, lightTangent));
    
    float3 toSurface = surfacePos - lightPos;
    float projDistance = dot(toSurface, lightNormal);
    
    float3 projPoint = surfacePos - lightNormal * projDistance;
    
    float3 localOffset = projPoint - lightPos;
    float u = dot(localOffset, lightTangent);
    float v = dot(localOffset, bitangent);
    
    u = clamp(u, -width * 0.5f, width * 0.5f);
    v = clamp(v, -height * 0.5f, height * 0.5f);
    
    float3 closestPoint = lightPos + lightTangent * u + bitangent * v;
    
    float3 toClosestPoint = closestPoint - surfacePos;
    float distToClosest = length(toClosestPoint);
    float3 lightDirFromClosest = normalize(toClosestPoint);
    
    float cosTheta = dot(surfaceNormal, lightDirFromClosest);
    if (cosTheta <= 0.0f)
    {
        specular = float3(0.0f, 0.0f, 0.0f);
        return float3(0.0f, 0.0f, 0.0f);
    }
    
    float lightCosTheta = dot(-lightDirFromClosest, lightNormal);
    if (lightCosTheta <= 0.0f)
    {
        specular = float3(0.0f, 0.0f, 0.0f);
        return float3(0.0f, 0.0f, 0.0f);
    }
    
    float3 diffuse;
    if (lightingMode == 1)
    {
        diffuse = CalculateLambert(surfaceNormal, -lightDirFromClosest);
    }
    else
    {
        diffuse = CalculateHalfLambert(surfaceNormal, -lightDirFromClosest);
    }
    
    specular = float3(0.0f, 0.0f, 0.0f);
    if (lightingMode == 3)
    {
        specular = CalculatePhongSpecular(surfaceNormal, lightDirFromClosest, toEye, shininess);
    }
    else if (lightingMode == 4)
    {
        specular = CalculateBlinnPhongSpecular(surfaceNormal, lightDirFromClosest, toEye, shininess);
    }
    
    float distSq = max(distToClosest * distToClosest, 0.01f);
    float attenuation = lightCosTheta / distSq;
    
    float falloffDistance = max(width, height) * 3.0f;
    float distanceRatio = distToClosest / falloffDistance;
    float distanceFalloff = 1.0f - saturate(distanceRatio);
    distanceFalloff = distanceFalloff * distanceFalloff;
    
    attenuation *= distanceFalloff;
    
    return diffuse * attenuation;
}