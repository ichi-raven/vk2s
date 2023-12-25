#ifndef BSDFS_GLSL_
#define BSDFS_GLSL_

precision highp float;
precision highp int;

#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable
#extension GL_EXT_control_flow_attributes : enable

#include "types.glsl"
#include "constants.glsl"
#include "randoms.glsl"

bool isReflection(const uint flags)
{
  return bool(flags & BSDF_FLAGS_REFLECTION);
}

bool isTransMission(const uint flags)
{
  return bool(flags & BSDF_FLAGS_TRANSMISSION);
}

bool isDiffuse(const uint flags)
{
  return bool(flags & BSDF_FLAGS_DIFFUSE);
}

bool isGlossy(const uint flags)
{
  return bool(flags & BSDF_FLAGS_GLOSSY);
}

bool isSpecular(const uint flags)
{
  return bool(flags & BSDF_FLAGS_SPECULAR);
}

bool IsNonSpecular(const uint flags) 
{
    return bool(flags & (BSDF_FLAGS_DIFFUSE | BSDF_FLAGS_GLOSSY)); 
}

float schlick(float cosine, float ref_idx) 
{
  float r0 = (1 - ref_idx) / (1 + ref_idx);
  r0 = r0 * r0;
  return r0 + (1 - r0) * pow((1 - cosine), 5);
}

vec3 local(ONB onb, vec3 pos)
{
  return pos.x * onb.u + pos.y * onb.v + pos.z * onb.w;
}

vec3 setFaceNormal(const vec3 rayDir, const vec3 outwardNormal)
{
  return dot(rayDir, outwardNormal) < 0 ? outwardNormal : -outwardNormal;
}

ONB buildFromW(vec3 w)
{
  ONB ret;
  ret.w = normalize(w);
  vec3 a = abs(ret.w.x) > 0.9 ? vec3(0, 1.0, 0) : vec3(1.0, 0, 0);
  ret.v = normalize(cross(ret.w, a));
  ret.u = cross(ret.w, ret.v);

  return ret;
}

vec3 lambertScatter(vec3 normal, inout uint prngState, out float pdf)
{
  ONB onb = buildFromW(normal);
  vec3 dir = local(onb, randomCosDirection(prngState));
  pdf = dot(onb.w, dir) / M_PI;
  return normalize(dir);
}

vec3 conductorScatter(vec3 wo, vec3 normal, float alpha, inout uint prngState, inout float pdf)
{
  // TODO:
  pdf = 1.0;

  return reflect(-wo, normalize(normal)) + alpha * randomUnitVector(prngState);
}

vec3 dielectricScatter(vec3 wo, vec3 normal, bool frontFace, float IOR, inout uint prngState, inout float pdf)
{
  float eta = frontFace ? 1.0 / IOR : IOR;

  const vec3 unitDir = -normalize(wo);

  const float cosine = min(dot(-unitDir, normal), 1.0);
  const float sine = sqrt(1.0 - cosine * cosine);
  const float reflectProb = schlick(cosine, eta);

  if (eta * sine > 1.0 || stepAndOutputRNGFloat(prngState) < reflectProb) 
  {
    return reflect(unitDir, normalize(normal));
  }

  // TODO:
  pdf = 1.0;
  return refract(unitDir, normalize(normal), eta);

}

BSDFSample sampleBSDF(const Material mat, const vec3 wo, const vec3 normal, inout uint prngState)
{
  BSDFSample ret;
  ret.f = mat.albedo.xyz;
  ret.pdf = 1.0;
  ret.flags = 0;
  ret.eta = 1.0;
  ret.pdfIsProportional = false;

  const vec3 faceNormal = setFaceNormal(-wo, normal);
  const bool front = faceNormal == normal;

  if (mat.matType == MAT_LAMBERT)
  {
    ret.flags = BSDF_FLAGS_DIFFUSE | BSDF_FLAGS_REFLECTION;
    
    ret.wi = lambertScatter(faceNormal, prngState, ret.pdf);
    ret.f *= M_INVPI * abs(dot(ret.wi, normal));
  }
  else if (mat.matType == MAT_CONDUCTOR)
  {
    // HACK:
    ret.flags = mat.alpha < 0.1 ? BSDF_FLAGS_SPECULAR : BSDF_FLAGS_GLOSSY;
    ret.flags |= BSDF_FLAGS_REFLECTION;
    ret.wi = conductorScatter(wo, faceNormal, mat.alpha, prngState, ret.pdf);
  }
  else if (mat.matType == MAT_DIELECTRIC)
  {
    ret.flags = BSDF_FLAGS_TRANSMISSION;
    // HACK:
    ret.flags |= mat.IOR == 1.0 ? BSDF_FLAGS_REFLECTION : 0;
    ret.wi = dielectricScatter(wo, faceNormal, front, mat.IOR, prngState, ret.pdf);
  }
  else
  {
    // ERROR
  }

  // prevent zero-division
  ret.pdf = max(ret.pdf, EPS);

  return ret;
}

#endif