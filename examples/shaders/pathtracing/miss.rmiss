#version 460
#extension GL_EXT_ray_tracing : enable

#include "types.glsl"
#include "constants.glsl"
#include "bindings.glsl"

layout(location = 0) rayPayloadInEXT Payload payload;

float sgn(float x)
{
  return x >= 0 ? 1.f : -1.f;
}

void main()
{
  const vec3 worldRayDirection = gl_WorldRayDirectionEXT;
  const float x = worldRayDirection.x;
  const float y = worldRayDirection.z;
  const float z = worldRayDirection.y;

  const float phi = sgn(y) * acos(x / sqrt(x * x + y * y));
  const float theta = acos(z / length(worldRayDirection));

  // account for infinite lights if ray has no intersection (missed)
  payload.L = vec3(0.1);
  //payload.L = texture(envmap, vec2(phi / M_PI2, theta / M_PI)).xyz;
  payload.end = true;
}