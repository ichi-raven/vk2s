#ifndef BINDINGS_GLSL_
#define BINDINGS_GLSL_

precision highp float;
precision highp int;

#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable
#extension GL_EXT_control_flow_attributes : enable

#include "../common/types.glsl"

// bindings
layout(binding=0, set=0) uniform accelerationStructureEXT TLAS;
layout(binding=1, set=0, rgba8) uniform image2D image;
layout(binding=2, set=0) uniform SceneParameters 
{
    mat4 mtxView;
    mat4 mtxProj;
    mat4 mtxViewInv;
    mat4 mtxProjInv;
    uint frame;
    uint spp;
    uint untilSPP;
    uint emitterSize;
} sceneParams;

layout(binding=3, set=0) readonly buffer InstanceMappings { InstanceMapping instanceMappings[]; };
layout(binding=4, set=0) readonly buffer Materials { Material materials[]; };
layout(binding=5, set=0) uniform sampler2D texSamplers[];
layout(binding=6, set=0) readonly buffer EmitterInfo 
{
  vec3 sceneCenter;
  float sceneRadius;
  uint32_t triEmitterNum;
  uint32_t pointEmitterNum;
  uint32_t infiniteEmitterExists;
  uint32_t activeEmitterNum;
} emitterInfo;
layout(binding=7, set=0) readonly buffer TriEmitters { TriEmitter triEmitters[]; };
layout(binding=8, set=0) readonly buffer InfiniteEmitterInfo { InfiniteEmitter infiniteEmitter; };
layout(binding=9, set=0, rgba32f) uniform image2D poolImage;

#endif