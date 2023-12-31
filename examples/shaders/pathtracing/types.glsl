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
  vec4 albedo;
  vec4 emissive;
  int32_t texIndex;
  int32_t matType;
  float alpha;
  float IOR;
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
  uint flags;
  float eta;
};

struct LightSample
{
  vec3 on;
  vec3 to;
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
};

// struct HitInfo
// {
//   vec3 albedo;
//   vec3 emitted;
//   vec3 worldPosition;
//   vec3 worldNormal;
//   bool endTrace;
//   int matType;
//   float alpha;
//   float IOR;
// };

struct ONB
{
  vec3 u;
  vec3 v;
  vec3 w;
};

#endif