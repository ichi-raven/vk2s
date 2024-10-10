#ifndef TYPES_GLSL_
#define TYPES_GLSL_

precision highp float;
precision highp int;

#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable
#extension GL_EXT_control_flow_attributes : enable

struct InstanceMapping
{
  uint64_t vertexBuffer;
  uint64_t indexBuffer;
  int32_t materialIndex;
  uint32_t padding_instanceMapping[3];
};

struct Material
{
  uint32_t matType;
  int32_t roughnessTex;
  vec2 roughness;

  vec4 albedo;

  vec3 eta;
  int32_t albedoTex;

  vec3 k;
  int32_t normalTex;

  vec4 emissive;
};

struct DisneyMaterial
{
  vec3 baseColor;
  float metallic;
  float roughness;
  float flatness;
  vec3 emissive;
    
  float specularTint;
  float specTrans;
  float diffTrans;
  float ior;
  float relativeIOR;
  float absorption;

  float sheen;
  vec3 sheenTint;
  float anisotropic;

  float clearcoat;
  float clearcoatGloss;
};

struct TriEmitter
{
  vec4 p[3];

  vec3 normal;
  float area;

  vec4 emissive;
};

struct InfiniteEmitter
{
  vec4 constantEmissive;
  uint32_t envmapIdx;
  uint32_t pdfIdx;
  uint32_t padding[2];
};

struct DisneyBSDFState
{
  bool isRefracted;
  bool hasBeenRefracted;
  float lastIOR;
};

struct Vertex 
{
  vec3 position;
  vec3 normal;
  vec2 texCoord;
  vec4 joint;
  vec4 weight;
};

struct Ray
{
  vec3 origin;
  vec3 direction;
};

struct BSDFSample
{
  vec3 f;
  vec3 wi;
  float pdf;
  vec3 eta;
  uint flags;
};

struct LightSample
{
  vec3 on;
  vec3 to;
  float G;
  vec3 L;
  float pdf;
};

struct Payload
{
  vec3 x;
  vec3 normal;
  BSDFSample bsdf;
  vec3 Le;
  uint prngState;
  bool intersected;
  bool emissive;
  float area;

  Material mat;
  // for Disney BSDF
  //DisneyBSDFState state;
  //DisneyMaterial mat;
};

struct ONB
{
  vec3 u;
  vec3 v;
  vec3 w;
};

#endif