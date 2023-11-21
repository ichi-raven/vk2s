layout(location = 0) rayPayloadEXT HitInfo
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
layout(binding=1, set=0, rgba8) uniform image2D image;
layout(binding=2, set=0) uniform SceneParameters 
{
    mat4 mtxView;
    mat4 mtxProj;
    mat4 mtxViewInv;
    mat4 mtxProjInv;
    float time;
    uint spp;
    uint seedMode;
    float padding;
} sceneParams;