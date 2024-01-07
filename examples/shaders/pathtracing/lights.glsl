#ifndef LIGHTS_GLSL_
#define LIGHTS_GLSL_

precision highp float;
precision highp int;

#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable
#extension GL_EXT_control_flow_attributes : enable

#include "types.glsl"
#include "constants.glsl"
#include "randoms.glsl"

bool intersectsLight(inout LightSample sampledLight, inout uint prngState)
{
    // const vec3 faceNormal = setFaceNormal(-gl_WorldRayDirectionEXT, worldNormal);

    // // Debug: for direct light sampling
    // vec3 debugLight[4] = {vec3(0.2300, 1.5800, -0.2200), vec3(0.2300, 1.5800, 0.1600), vec3(-0.2400, 1.5800, 0.1600), vec3(-0.2400, 1.5800, -0.2200)};    
    // const float xRange[2] = {debugLight[2].x, debugLight[0].x};
    // const float zRange[2] = {debugLight[0].z, debugLight[1].z};
    // const float lightArea = (xRange[1] - xRange[0]) * (zRange[1] - zRange[0]);

    // // sample direct illumination
    // const float r1 = stepAndOutputRNGFloat(payload.seed);
    // const float r2 = stepAndOutputRNGFloat(payload.seed);
    // const float randLightX = r1 * (xRange[1] - xRange[0]) + xRange[0];
    // const float randLightZ = r2 * (zRange[1] - zRange[0]) + zRange[0];
    // const vec3 onLight = vec3(randLightX, debugLight[0].y, randLightZ);
    // //const vec3 onLight = vec3((xRange[1] + xRange[0]) * 0.5, debugLight[0].y, (zRange[1] + zRange[0]) * 0.5);
    // vec3 toLight = onLight - worldPos;
    // const float distSq = dot(toLight, toLight);
    
    // toLight = normalize(toLight);
    // const float lightCos = abs(toLight.y);

    // if (dot(toLight, worldNormal) > 0.0 && lightCos > EPS)
    // {
    //     sampledLight.f = bsdf.f;
    //     sampledLight.pdf = max(distSq / (lightCos * lightArea), EPS);
    //     sampledLight.L = vec3(10.0);//shadowed ? vec3(0.0) : vec3(10.0);
    // }

    //return dot(toLight, worldNormal) > 0.0 && lightCos > EPS

    // debug
    return true;

}

#endif