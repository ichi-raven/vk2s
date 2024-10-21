
//import types;
#include "./types.slang"

struct InfiniteEmitter
{
    float4 constantEmissive;
    uint32_t envmapIdx;
    uint32_t pdfIdx;
    uint32_t padding[2];
};

[[vk::binding(8, 0)]] StructuredBuffer<InfiniteEmitter> infiniteEmitter;

[shader("miss")]
void main(inout Payload payload)
{
    payload.Le          = infiniteEmitter[0].constantEmissive.xyz;
    payload.intersected = false;
    payload.emissive    = dot(payload.Le, payload.Le) > 1e-6;
}