static const uint INVALID_PARTICLE_INDEX = 0xffffffffu;
static const uint RESERVED_PARTICLE_INDEX = 0xfffffffeu;

struct GpuParticleState
{
    float4 positionAndActive;
    float4 velocityAndLifetime;
    uint ownerParticleIndex;
    float age;
    float initialLifetime;
    uint padding;
};

struct GpuParticleMotion
{
    float4 forceAndDrag;
    float4 velocityAndSpeedModifier;
    float4 limitAndGravity;
    float4 noiseParams;
};

struct GpuSpawnRequest
{
    GpuParticleState state;
    GpuParticleMotion motion;
    uint operation;
    uint3 padding;
};

cbuffer SimulationSettings : register(b0)
{
    float4x4 gViewProjection;
    float4 gCameraPosition;
    float4 gCameraRight;
    float4 gCameraUp;
    float4 gCameraForward;
    float4 gRenderParams;
    float4 gCameraFadeParams;
    float4 gAttractorPosition;
    float4 gAttractorParams;
    float4 gSimulationOriginAndLocal;
    float4 gSimulationRotation;
    uint gParticleCount;
    uint gParticleCapacity;
    uint gSpawnRequestCount;
    uint gPadding;
};

StructuredBuffer<GpuSpawnRequest> gSpawnRequests : register(t0);
RWStructuredBuffer<GpuParticleState> gStates : register(u0);
RWStructuredBuffer<GpuParticleMotion> gMotions : register(u1);
RWStructuredBuffer<uint> gAliveFlags : register(u2);
RWStructuredBuffer<uint> gFreeList : register(u3);
RWByteAddressBuffer gFreeCount : register(u4);
RWStructuredBuffer<uint> gOwnerMappings : register(u5);

uint PopFreeParticle()
{
    // Update CSとの間のUAV barrierにより、counterが示すslotは必ず公開済みである。
    // 同一Emitter dispatch内ではpushしないため、CASによるcounter予約だけで競合を解決できる。
    uint availableCount = gFreeCount.Load(0u);
    while (availableCount > 0u)
    {
        uint observedCount;
        gFreeCount.InterlockedCompareExchange(0u, availableCount, availableCount - 1u, observedCount);
        if (observedCount == availableCount)
        {
            return gFreeList[availableCount - 1u];
        }
        availableCount = observedCount;
    }
    return INVALID_PARTICLE_INDEX;
}

[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint requestIndex = dispatchThreadId.x;
    if (requestIndex >= gSpawnRequestCount)
    {
        return;
    }

    GpuSpawnRequest request = gSpawnRequests[requestIndex];
    uint ownerIndex = request.state.ownerParticleIndex;
    if (ownerIndex >= gParticleCapacity)
    {
        return;
    }

    if (request.operation == 1u)
    {
        uint existingIndex = gOwnerMappings[ownerIndex];
        if (existingIndex < gParticleCapacity && gAliveFlags[existingIndex] != 0u)
        {
            GpuParticleState state = gStates[existingIndex];
            state.positionAndActive.xyz = request.state.positionAndActive.xyz;
            state.velocityAndLifetime.xyz = request.state.velocityAndLifetime.xyz;
            gStates[existingIndex] = state;
            gMotions[existingIndex] = request.motion;
        }
        return;
    }

    // ownerを先に予約し、同一ownerへの重複要求がFreeListを消費しないようにする。
    uint previousMapping;
    InterlockedCompareExchange(
        gOwnerMappings[ownerIndex], INVALID_PARTICLE_INDEX, RESERVED_PARTICLE_INDEX, previousMapping);
    if (previousMapping != INVALID_PARTICLE_INDEX)
    {
        return;
    }

    uint particleIndex = PopFreeParticle();
    if (particleIndex == INVALID_PARTICLE_INDEX)
    {
        uint ignored;
        InterlockedCompareExchange(
            gOwnerMappings[ownerIndex], RESERVED_PARTICLE_INDEX, INVALID_PARTICLE_INDEX, ignored);
        return;
    }

    uint previousAlive;
    InterlockedCompareExchange(gAliveFlags[particleIndex], 0u, 1u, previousAlive);
    if (previousAlive != 0u)
    {
        uint ignored;
        InterlockedCompareExchange(
            gOwnerMappings[ownerIndex], RESERVED_PARTICLE_INDEX, INVALID_PARTICLE_INDEX, ignored);
        return;
    }
    // FreeList破損時にも生存粒子を壊さないよう、aliveの所有権を得てから状態を公開する。
    gStates[particleIndex] = request.state;
    gMotions[particleIndex] = request.motion;
    InterlockedExchange(gOwnerMappings[ownerIndex], particleIndex, previousMapping);
}
