#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable
#extension GL_EXT_nonuniform_qualifier : enable

hitAttributeEXT vec3 attribs;

layout(location = 0) rayPayloadInEXT HitInfo
{
  vec3 color;
  vec3 worldPosition;
  vec3 worldNormal;
  bool endTrace;
  int matType;
  float alpha;
  float IOR;
} hitInfo;

layout(binding=0, set=0) uniform accelerationStructureEXT topLevelAS;
layout(binding=2, set=0) uniform SceneParameters 
{
    mat4 mtxView;
    mat4 mtxProj;
    mat4 mtxViewInv;
    mat4 mtxProjInv;
    float time;
    uint spp;
    vec2 padding_sceneParams;
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

struct Vertex 
{
  vec3 position;
  vec3 normal;
  vec2 texCoord;
  vec4 joint;
  vec4 weight;
};

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
  const Material material = materials[nonuniformEXT(mapping.materialIndex)];
  const Vertex vtx = FetchVertexInterleaved(barys, mapping.vertexBuffer, mapping.indexBuffer);

  const vec3 red = vec3(0.7, 0.2, 0.2);
  const vec3 green = vec3(0.2, 0.7, 0.2);
  const vec3 blue = vec3(0.2, 0.2, 0.7);

  vec3 worldNormal = mat3(gl_ObjectToWorldEXT) * vtx.normal;
  vec3 vtxColor = material.albedo.xyz;
  if (material.texIndex != -1)
  {
    vtxColor = texture(texSamplers[nonuniformEXT(material.texIndex)], vtx.texCoord).xyz;
  }

  hitInfo.color = vtxColor;
  hitInfo.worldPosition = mat3(gl_ObjectToWorldEXT) * vtx.position;
  hitInfo.worldNormal = mat3(gl_ObjectToWorldEXT) * vtx.normal;
  hitInfo.endTrace = material.matType == 3; // if emitter, end trace
  hitInfo.alpha = material.alpha;
  hitInfo.IOR = material.IOR;
  hitInfo.matType = material.matType;

  return;
}
