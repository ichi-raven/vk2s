#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_control_flow_attributes : enable

hitAttributeEXT vec3 attribs;

#include "types.glsl"
#include "constants.glsl"
#include "bindings.glsl"
#include "randoms.glsl"
#include "BSDFs.glsl"
#include "lights.glsl"

layout(location = 0) rayPayloadInEXT Payload payload;
layout(location = 1) rayPayloadEXT bool shadowed;

layout(buffer_reference, buffer_reference_align = 4, scalar) readonly buffer VertexBuffer 
{
    Vertex v[];
};

layout(buffer_reference, scalar) readonly buffer IndexBuffer 
{
    uvec3 i[];
};

Vertex FetchVertexInterleaved(
  vec3 barys,
  uint64_t vertexBuffer,
  uint64_t indexBuffer)
{
  IndexBuffer indices = IndexBuffer(indexBuffer);
  VertexBuffer verts = VertexBuffer(vertexBuffer);

  const uvec3 idx = indices.i[gl_PrimitiveID].xyz;
  Vertex v0 = verts.v[idx.x];
  Vertex v1 = verts.v[idx.y];
  Vertex v2 = verts.v[idx.z];

  Vertex v;
  v.position = v0.position * barys.x + v1.position * barys.y + v2.position * barys.z;
  v.normal = normalize(v0.normal * barys.x + v1.normal * barys.y + v2.normal * barys.z);
  v.texCoord = v0.texCoord * barys.x + v1.texCoord * barys.y + v2.texCoord * barys.z;
  v.joint = v0.joint * barys.x + v1.joint * barys.y + v2.joint * barys.z;
  v.weight = v0.weight * barys.x + v1.weight * barys.y + v2.weight * barys.z;

  return v;
}

void main() 
{

  const vec3 barys = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
  const InstanceMapping mapping = instanceMappings[gl_InstanceID];
  const Vertex vtx = FetchVertexInterleaved(barys, mapping.vertexBuffer, mapping.indexBuffer);

  Material material = materials[nonuniformEXT(mapping.materialIndex)];
  if (material.texIndex != -1)
  {
    material.albedo = texture(texSamplers[nonuniformEXT(material.texIndex)], vtx.texCoord);
  }

  const vec3 worldPos    = (gl_ObjectToWorldEXT * vec4(vtx.position, 1.0)).xyz;
  const vec3 worldNormal = mat3(gl_ObjectToWorldEXT) * vtx.normal;

  payload.ray.origin = worldPos;
  payload.normal = worldNormal;

  // sample BSDF
  const BSDFSample bsdf = sampleBSDF(material, -gl_WorldRayDirectionEXT, worldNormal, payload.prngState);
  // account for emissive surface if light was not sampled
  payload.Le = material.emissive.xyz;
  payload.ray.direction = bsdf.wi;
  payload.beta = bsdf.f / bsdf.pdf;
  payload.specularBounce = isSpecular(bsdf.flags);
  payload.sampledLight = false;

  // light sampling
  //stepAndOutputRNGFloat(payload.prngState) > 0.0 && 
  if (material.matType == 0) // light sample
  {
    //payload.sampledLight = intersectsLight(payload.ls, payload.prngState);

    const vec3 faceNormal = setFaceNormal(-gl_WorldRayDirectionEXT, worldNormal);

    // Debug: for direct light sampling
    vec3 debugLight[4] = {vec3(0.2300, 1.5800, -0.2200), vec3(0.2300, 1.5800, 0.1600), vec3(-0.2400, 1.5800, 0.1600), vec3(-0.2400, 1.5800, -0.2200)};    
    const float xRange[2] = {debugLight[2].x, debugLight[0].x};
    const float zRange[2] = {debugLight[0].z, debugLight[1].z};
    const float lightArea = (xRange[1] - xRange[0]) * (zRange[1] - zRange[0]);

    // sample direct illumination
    const float r1 = stepAndOutputRNGFloat(payload.prngState);
    const float r2 = stepAndOutputRNGFloat(payload.prngState);
    const float randLightX = r1 * (xRange[1] - xRange[0]) + xRange[0];
    const float randLightZ = r2 * (zRange[1] - zRange[0]) + zRange[0];
    const vec3 onLight = vec3(randLightX, debugLight[0].y, randLightZ);
    //const vec3 onLight = vec3((xRange[1] + xRange[0]) * 0.5, debugLight[0].y, (zRange[1] + zRange[0]) * 0.5);
    vec3 toLight = onLight - worldPos;
    const float distSq = dot(toLight, toLight);
    
    toLight = normalize(toLight);
    const float lightCos = max(EPS, abs(toLight.y));

    // cast shadow ray
    const uint flags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
    shadowed = true;
    traceRayEXT(TLAS, flags, 0xFF, 0, 0, 1, worldPos, tmin, toLight, sqrt(distSq), 1);
    // // DEBUG
    // payload.end = true;
    // payload.Le = shadowed ? BLACK : WHITE;
    // return;

    if (dot(toLight, worldNormal) > 0.0 && lightCos > EPS && shadowed)
    {
      payload.sampledLight = true;
      payload.ls.f = bsdf.f;
      payload.ls.pdf = max(distSq / (lightCos * lightArea), EPS);
      payload.ls.L = vec3(10.0);//shadowed ? vec3(0.0) : vec3(10.0);
    }
  }

  // maybe need russian roulette
  payload.end = false;
  
  return;
}
