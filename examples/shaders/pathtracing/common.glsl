#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable

layout(binding=0, set=0) uniform accelerationStructureEXT topLevelAS;
layout(binding=1, set=0, rgba8) uniform image2D image;
layout(binding=2, set=0) uniform SceneParameters 
{
    mat4 mtxView;
    mat4 mtxProj;
    mat4 mtxViewInv;
    mat4 mtxProjInv;
    float time;
    uint spp;
    uint seedMode;
    float padding_sceneParam;
} sceneParams;

struct InstanceMapping
{
  uint64_t vertexBuffer;
  uint64_t indexBuffer;
  int32_t materialIndex;
  uint32_t padding_instanceMapping[3];
};

struct Material
{
  vec4 albedo;
  int32_t texIndex;
  int32_t matType;
  float alpha;
  float IOR;
};

layout(binding=3, set=0) readonly buffer InstanceMappings { InstanceMapping instanceMappings[]; };

layout(binding=4, set=0) readonly buffer Materials { Material materials[]; };

layout(binding=5, set=0) uniform sampler2D texSamplers[];

layout(binding=6, set=0) uniform sampler2D envmap;

struct Vertex 
{
  vec3 position;
  vec3 normal;
  vec2 texCoord;
  vec4 joint;
  vec4 weight;
};

struct HitInfo{
  vec3 color;
  vec3 worldPosition;
  vec3 worldNormal;
  bool endTrace;
  int matType;
  float alpha;
  float IOR;
};

// constants
#define M_PI  (3.1415926535897932384626433832795)
#define M_PI2 (6.28318530718)

// functions
float stepAndOutputRNGFloat(inout uint rngState)
{
  // Condensed version of pcg_output_rxs_m_xs_32_32, with simple conversion to floating-point [0,1].
  rngState  = rngState * 747796405 + 1;
  uint word = ((rngState >> ((rngState >> 28) + 4)) ^ rngState) * 277803737;
  word      = (word >> 22) ^ word;
  return float(word) / 4294967295.0f;
}

vec3 randomUnitVector(inout uint rngState) 
{
  float a = stepAndOutputRNGFloat(rngState) * 2. * M_PI;
  float z = stepAndOutputRNGFloat(rngState) * 2. - 1.;
  float r = sqrt(1 - z * z);
  return vec3(r * cos(a), r * sin(a), z);
}

float schlick(float cosine, float ref_idx) 
{
  float r0 = (1 - ref_idx) / (1 + ref_idx);
  r0 = r0 * r0;
  return r0 + (1 - r0) * pow((1 - cosine), 5);
}
