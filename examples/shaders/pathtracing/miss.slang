
struct BSDFSample
{
  float3 f;
  float3 wi;
  float pdf;
  float3 eta;
  uint flags;
};

struct Material
{
    uint32_t matType;
    int32_t roughnessTex;
    float2 roughness;

    float4 albedo;

    float3 eta;
    int32_t albedoTex;

    float3 k;
    int32_t normalTex;

    float4 emissive;
};

struct Payload
{
  float3 x;
  float3 normal;
  BSDFSample bsdf;
  float3 Le;
  uint prngState;
  bool intersected;
  bool emissive;
  float area;

  Material mat;
  // for Disney BSDF
  //DisneyBSDFState state;
  //DisneyMaterial mat;
};

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