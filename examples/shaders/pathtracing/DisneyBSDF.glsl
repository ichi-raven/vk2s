#ifndef DISNEYBSDFS_GLSL_
#define DISNEYBSDFS_GLSL_

precision highp float;
precision highp int;

#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable
#extension GL_EXT_control_flow_attributes : enable

#include "types.glsl"
#include "constants.glsl"
#include "randoms.glsl"

// Sheen-------------------------------------
vec3 calculateTint(const vec3 baseColor)
{
    // -- The color tint is never mentioned in the SIGGRAPH presentations as far as I recall but it was done in
    // --  the BRDF Explorer so I'll replicate that here.
    float luminance = dot(vec3(0.3f, 0.6f, 1.0f), baseColor);
    return (luminance > 0.0f) ? baseColor * (1.0f / luminance) : vec3(1.0);
}

vec3 evaluateSheen(const vec3 baseColor, const float sheen, const vec3 sheenTint, const vec3 wo, const vec3 wm, const vec3 wi)
{
    if(surface.sheen <= 0.0) 
    {
        return vec3(0.0);
    }

    const float dotHL = dot(wm, wi);
    const vec3 tint = calculateTint(baseColor);
    return sheen * mix(vec3(1.0), tint, sheenTint) * pow((1.0 - dotHL), 5);
}

// Clearcoat-------------------------------------
float D_GTR1(const float absDotHL, const float a)
{
    if(a >= 1.0) 
    {
        return M_INVPI;
    }

    const float a2 = a * a;
    return (a2 - 1.0f) / (M_PI * log2(a2) * (1.0 + (a2 - 1.0) * absDotHL * absDotHL));
}

float separableSmithGGXG1(const vec3 w, const float a)
{
    const float a2 = a * a;
    const float absDotNV = abs(cos(w));

    return 2.0 / (1.0 + sqrt(a2 + (1 - a2) * absDotNV * absDotNV));
}

float EvaluateDisneyClearcoat(const float clearcoat, const float alpha, const vec3 wo, const vec3 wm,
                                     const vec3 wi, out float fPdfW, out float rPdfW)
{
    if(clearcoat <= 0.0) 
    {
        return 0.0;
    }

    float absDotNH = abs(cos(wm));
    float absDotNL = abs(cos(wi));
    float absDotNV = abs(cos(wo));
    float dotHL = dot(wm, wi);

    float d = D_GTR1(absDotNH, mix(0.1, 0.001, alpha));
    float f = schlick(0.04, dotHL);
    float gl = separableSmithGGXG1(wi, 0.25);
    float gv = separableSmithGGXG1(wo, 0.25);

    fPdfW = d / (4.0 * absDotNL);
    rPdfW = d / (4.0 * absDotNV);

    return 0.25 * clearcoat * d * f * gl * gv;
}

// Specular-------------------------------------
float GGXAnisotropicD(const vec3 wm, const float ax, const float ay)
{
    const float dotHX2 = wm.x * wm.x;
    const float dotHY2 = wm.z * wm.z;
    const float cosTheta = cos(wm);
    const float cos2Theta = cosTheta * cosTheta;
    const float ax2 = ax * ax;
    const float ay2 = ay * ay;

    const tmp = dotHX2 / ax2 + dotHY2 / ay2 + cos2Theta;
    return 1.0 / (M_PI * ax * ay * tmp * tmp);
}



BSDFSample sampleDisneyBSDF(const DisneyMaterial mat, const vec3 x, const vec3 normal, inout DisneyBSDFState state, inout uint prngState)
{
    // init
    BSDFSample ret;
    ret.f = mat.albedo;
    ret.pdf = 1.0;
    ret.flags = 0;
    ret.eta = 1.0;



    return ret;
}

#endif