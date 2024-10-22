module types;

public struct BSDFSample
{
  public float3 f;
  public float3 wi;
  public float pdf;
  public float3 eta;
  public uint flags;
};

public struct Material
{
    public uint32_t matType;
    public int32_t roughnessTex;
    public float2 roughness;

    public float4 albedo;

    public float3 eta;
    public int32_t albedoTex;

    public float3 k;
    public int32_t normalTex;

    public float4 emissive;
};

public struct Payload
{
  public float3 x;
  public float3 normal;
  public BSDFSample bsdf;
  public float3 Le;
  public uint prngState;
  public bool intersected;
  public bool emissive;
  public float area;

  public Material mat;
};