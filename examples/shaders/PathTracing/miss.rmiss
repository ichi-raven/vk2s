#version 460
#extension GL_EXT_ray_tracing : enable
//#extension GL_EXT_debug_printf : enable


layout(location = 0) rayPayloadInEXT HitInfo
{
  vec3 color;
  vec3 worldPosition;
  vec3 worldNormal;
  bool endTrace;
} hitInfo;

layout(binding=6, set=0) uniform sampler2D envmap;

float sgn(float x)
{
  return x >= 0 ? 1.f : -1.f;
}

#define M_PI  (3.1415926535897932384626433832795)
#define M_PI2 (6.28318530718)

void main()
{
  const vec3 worldRayDirection = gl_WorldRayDirectionEXT;
  const float x = worldRayDirection.x;
  const float y = worldRayDirection.z;
  const float z = worldRayDirection.y;

  const float phi = sgn(y) * acos(x / sqrt(x * x + y * y));
  const float theta = acos(z / length(worldRayDirection));

  hitInfo.color = texture(envmap, vec2(phi / M_PI2, theta / M_PI)).xyz;
  hitInfo.endTrace = true;
}