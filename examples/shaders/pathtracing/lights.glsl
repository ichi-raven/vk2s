#ifndef LIGHTS_GLSL_
#define LIGHTS_GLSL_

precision highp float;
precision highp int;

#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable
#extension GL_EXT_control_flow_attributes : enable

#include "../common/types.glsl"
#include "../common/constants.glsl"
#include "../common/randoms.glsl"
#include "bindings.glsl"

LightSample sampleTriLight(const TriEmitter emitter, const vec3 pos, inout uint prngState)
{

    LightSample ls;
    ls.pdf = 1.0;

    // sample point from emitter
    float r1 = stepAndOutputRNGFloat(prngState);
    float r2 = stepAndOutputRNGFloat(prngState);
    if (r1 + r2 > 1.0)
    {
        r1 = (1.0 - r1);
        r2 = (1.0 - r2);
    }

    ls.L = emitter.emissive.xyz;
    ls.on = (1.0 - r1 - r2) * emitter.p[0].xyz + r1 * emitter.p[1].xyz + r2 * emitter.p[2].xyz;
    ls.to = ls.on - pos;
    const float distSq = dot(ls.to, ls.to);
    const float lightCos = abs(dot(normalize(ls.to), emitter.normal));
    ls.G = lightCos / distSq;
    ls.pdf = 1. / emitter.area / emitterInfo.triEmitterNum;

    return ls;
}

LightSample sampleInfiniteLight(const vec3 pos, inout uint prngState)
{
    LightSample ls;
    ls.pdf = 1.0;

    if(infiniteEmitter.envmapIdx == -1) // constant emitter
    {
        vec3 wi = randomUnitVector(prngState);
        ls.L = infiniteEmitter.constantEmissive.xyz;
        ls.pdf = 0.25 * M_INVPI;
        ls.on = emitterInfo.sceneRadius * 2. * wi;
        ls.to = ls.on - pos;
        // if l -> \infty, the G term will be canceled
        ls.G = 1.0;
        //const float lightCos = abs(dot(normalize(ls.to), normalize(ls.on - emitterInfo.sceneCenter)));
        //ls.G = lightCos / dot(ls.to, ls.to);
    }
    else // envmap emitter
    {
        // TODO
    }

    return ls;
}

LightSample sampleLight(const vec3 pos, inout uint prngState)
{
    // sample one emitter (uniform)
    float emitterTypeRnd = stepAndOutputRNGFloat(prngState);

    // if (emitterTypeRnd < 1. / float(emitterInfo.activeEmitterNum))
    // {
    //     const uint idx = uint(stepAndOutputRNGFloat(prngState) * emitterInfo.triEmitterNum);
    //     LightSample ls = sampleTriLight(triEmitters[idx], pos, prngState);
    //     ls.pdf /= float(emitterInfo.activeEmitterNum);
    //     return ls;
    // }
    // else
    // {
        LightSample ls = sampleInfiniteLight(pos, prngState);
        //ls.pdf /= float(emitterInfo.activeEmitterNum);
        return ls;
    //}
}

float calcHitLightPdf(const vec3 x, const vec3 lightPos, const vec3 lightNormal, const float lightArea, out float invG)
{
    const vec3 to = lightPos - x;
    const float distSq = dot(to, to);

    const float lightCos = abs(dot(normalize(to), lightNormal));
    
    invG = distSq / lightCos;
    return 1. / lightArea / emitterInfo.triEmitterNum;// / float(emitterInfo.activeEmitterNum);
}

float calcInfiniteLightPdf(out float invG)
{
    invG = 1.0;
    return 0.25 * M_INVPI;// / float(emitterInfo.activeEmitterNum);
}

#endif