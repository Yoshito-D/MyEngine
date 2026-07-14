static const uint INVALID_PARTICLE_INDEX = 0xffffffffu;

struct GpuParticleState
{
    float4 positionAndActive;
    float4 velocityAndLifetime;
    uint ownerParticleIndex;
    float age;
    float initialLifetime;
    uint padding;
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
    uint gInitializationStartIndex;
};

RWStructuredBuffer<GpuParticleState> gStates : register(u0);
RWStructuredBuffer<uint> gAliveFlags : register(u1);
RWStructuredBuffer<uint> gFreeList : register(u2);
RWByteAddressBuffer gFreeCount : register(u3);
RWStructuredBuffer<uint> gOwnerMappings : register(u4);

[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint localIndex = dispatchThreadId.x;
    uint index = gInitializationStartIndex + localIndex;
    if (index >= gParticleCapacity)
    {
        return;
    }

    GpuParticleState state = (GpuParticleState)0;
    state.ownerParticleIndex = INVALID_PARTICLE_INDEX;
    gStates[index] = state;

    uint previousValue;
    InterlockedExchange(gAliveFlags[index], 0u, previousValue);
    InterlockedExchange(gOwnerMappings[index], INVALID_PARTICLE_INDEX, previousValue);

    if (gInitializationStartIndex == 0u)
    {
        // stackの末尾から0,1,2...の順にpopでき、再現性のある初回割り当てになる。
        gFreeList[index] = gParticleCapacity - 1u - index;
        if (index == 0u)
        {
            gFreeCount.InterlockedExchange(0u, gParticleCapacity, previousValue);
        }
    }
    else
    {
        // 実行中の容量拡張では既存状態を保持し、新規slotだけをatomicに公開する。
        uint freeSlot;
        gFreeCount.InterlockedAdd(0u, 1u, freeSlot);
        if (freeSlot < gParticleCapacity)
        {
            gFreeList[freeSlot] = index;
        }
        else
        {
            uint ignored;
            gFreeCount.InterlockedAdd(0u, 0xffffffffu, ignored);
        }
    }
}
