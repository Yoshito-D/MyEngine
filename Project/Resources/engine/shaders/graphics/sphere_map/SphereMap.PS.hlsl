#include "SphereMap.hlsli"

static const float kPi = 3.14159265f;
static const float kLatitudeIntervals = 10.0f;
static const float kMeridianCount = 20.0f;
static const float kWireHalfWidth = 0.55f; // 線の芯の半幅（UIピクセル）。
static const float kWireGlowReach = 2.7f;  // 線から広がる光の幅（UIピクセル）。
static const float kBackWireIntensity = 0.34f; // 裏側の線の明るさ。手前の線との明暗差で奥行きを示す。
static const float kRimGlowReach = 4.0f; // 円周の周りに光が広がる幅（UIピクセル）。

float Glow(float distancePixels) {
   // 線から離れるほど光を弱める。
   return exp2(-0.24f * distancePixels * distancePixels);
}

float WireLight(float lineDistance, float emphasis) {
   const float core = 1.0f - smoothstep(0.12f, kWireHalfWidth, lineDistance);
   const float halo = Glow(lineDistance) * (1.0f - smoothstep(kWireHalfWidth, kWireGlowReach, lineDistance));
   return core * 0.63f + halo * lerp(0.30f, 0.46f, emphasis);
}

float SphereWires(float3 normal, float samples, float emphasis) {
   const float latitude = asin(clamp(normal.y, -1.0f, 1.0f));
   const float latitudePhase = latitude * (kLatitudeIntervals / kPi);
   const float latitudeRate = length(float2(ddx(latitudePhase), ddy(latitudePhase))) * samples;
   const float latitudeDistance = abs(latitudePhase - round(latitudePhase)) / max(latitudeRate, 0.002f);
   const float latitudeLight = WireLight(latitudeDistance, emphasis) *
      (1.0f - smoothstep(0.55f, 1.15f, latitudeRate));

   const float2 equator = normal.xz;
   const float meridianStep = 2.0f * kPi / kMeridianCount;
   const float longitude = atan2(equator.y, equator.x + 1.0e-6f);
   const float meridian = round(longitude / meridianStep) * meridianStep;
   const float2 planeNormal = float2(-sin(meridian), cos(meridian));
   // 経線を通る平面への距離を使い、経度が定まらない極でも全経線を連続して合流させる。
   const float2 meridianGradient = float2(dot(planeNormal, ddx(equator)), dot(planeNormal, ddy(equator)));
   const float meridianDistance = abs(dot(planeNormal, equator)) / max(length(meridianGradient) * samples, 1.0e-5f);
   return max(latitudeLight, WireLight(meridianDistance, emphasis));
}

float4 main(MapVertex input) : SV_Target0 {
   const float samples = input.axisY.w;
   const float2 fromMapCenter = input.position.xy / samples - input.aperture.xy;
   const float apertureEdge = input.aperture.z - length(fromMapCenter);

   if (input.sphere.w < 0.0f) {
      const float edgeDistance = abs(apertureEdge);
      const float core = 1.0f - smoothstep(0.3f, 1.0f, edgeDistance);
      const float glow = Glow(edgeDistance) * (1.0f - smoothstep(1.0f, kRimGlowReach, edgeDistance));
      return float4(input.tint.rgb, (core * 0.65f + glow * 0.35f) * input.tint.a);
   }

   if (input.sphere.w == 0.0f) {
      // 遮蔽時も光点の中心部分は読み取れる明るさに保つ。
      const float radialSquared = dot(input.local, input.local);
      const float radius = sqrt(radialSquared);
      const float core = 1.0f - smoothstep(0.24f, 0.38f, radius);
      const float glow = exp(-3.8f * radialSquared) * (1.0f - smoothstep(0.66f, 1.0f, radius));
      const float3 light = lerp(input.tint.rgb, float3(1.0f, 1.0f, 1.0f), core * 0.45f);
      return float4(light, saturate((glow * 0.88f + core) * input.axisX.w) * input.tint.a);
   }

   // 透視カメラからの視線と球面との交点を両側で求め、裏面も薄く透けて見えるようにする。
   const float3 ray = normalize(float3(input.local, 1.0f));
   const float nearest = dot(ray, input.sphere.xyz);
   const float3 offset = ray * nearest - input.sphere.xyz;
   const float radius = input.sphere.w;
   const float discriminant = radius * radius - dot(offset, offset);
   const float halfChordLength = sqrt(max(discriminant, 0.0f));
   const float3 halfChord = ray * halfChordLength;
   // ミニマップはYが下向きなので、外積の順序を逆にして惑星ローカルのZ軸を復元する。
   const float3x3 viewToPlanet = float3x3(input.axisX.xyz, input.axisY.xyz, cross(input.axisY.xyz, input.axisX.xyz));
   const float3 localOffset = mul(viewToPlanet, offset) / radius;
   const float3 localHalfChord = mul(viewToPlanet, halfChord) / radius;
   const float emphasis = input.axisX.w;
   const float nearDistance = input.aperture.w / ray.z;
   const float frontWires = SphereWires(localOffset - localHalfChord, samples, emphasis) *
      step(nearDistance, nearest - halfChordLength);
   const float backWires = SphereWires(localOffset + localHalfChord, samples, emphasis) *
      step(nearDistance, nearest + halfChordLength);

   // 球面を外れる位置では発光せず、接線付近の線は約1 UIピクセルの範囲で滑らかに消す。
   const float edgeWidth = length(float2(ddx(discriminant), ddy(discriminant))) * samples;
   const float coverage = smoothstep(0.0f, max(edgeWidth, 1.0e-5f), discriminant);
   const float wires = saturate(frontWires + backWires * kBackWireIntensity);
   return float4(input.tint.rgb, wires * coverage * saturate(apertureEdge) * input.tint.a);
}
