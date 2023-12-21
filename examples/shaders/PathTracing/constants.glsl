#ifndef CONSTANTS_GLSL_
#define CONSTANTS_GLSL_

precision highp float;
precision highp int;

#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable
#extension GL_EXT_control_flow_attributes : enable

// constants
#define EPS (0.001)

#define M_PI  (3.1415926535897932384626433832795)
#define M_PI2 (6.28318530718)
#define M_INVPI (0.31830988618)

#define MAT_LAMBERT (0)
#define MAT_CONDUCTOR (1)
#define MAT_DIELECTRIC (2)

#define MAX_DEPTH (8)

#define BSDF_FLAGS_REFLECTION   (1 << 0)
#define BSDF_FLAGS_TRANSMISSION (1 << 1)
#define BSDF_FLAGS_DIFFUSE      (1 << 2)
#define BSDF_FLAGS_GLOSSY       (1 << 3)
#define BSDF_FLAGS_SPECULAR     (1 << 4)

const float tmin = 0.00;
const float tmax = 10000.0;
const float offset = EPS;

#endif