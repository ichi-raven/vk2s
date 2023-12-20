#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable
#extension GL_EXT_control_flow_attributes : enable

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
  vec4 emissive;
  int32_t texIndex;
  int32_t matType;
  float alpha;
  float IOR;
};

struct Vertex 
{
  vec3 position;
  vec3 normal;
  vec2 texCoord;
  vec4 joint;
  vec4 weight;
};

struct Ray
{
  vec3 origin;
  vec3 direction;
};

struct HitInfo
{
  vec3 albedo;
  vec3 emitted;
  vec3 worldPosition;
  vec3 worldNormal;
  bool endTrace;
  int matType;
  float alpha;
  float IOR;
};

struct BSDFSample
{
  vec3 f;
  vec3 wi;
  float pdf;
  uint flags;
  float eta;
  bool pdfIsProportional;
};

struct ONB
{
  vec3 u;
  vec3 v;
  vec3 w;
};

// bindings
layout(binding=0, set=0) uniform accelerationStructureEXT topLevelAS;
layout(binding=1, set=0, rgba8) uniform image2D image;
layout(binding=2, set=0) uniform SceneParameters 
{
    mat4 mtxView;
    mat4 mtxProj;
    mat4 mtxViewInv;
    mat4 mtxProjInv;
    uint frame;
    uint spp;
    uint untilSPP;
    
} sceneParams;

layout(binding=3, set=0) readonly buffer InstanceMappings { InstanceMapping instanceMappings[]; };
layout(binding=4, set=0) readonly buffer Materials { Material materials[]; };
layout(binding=5, set=0) uniform sampler2D texSamplers[];
layout(binding=6, set=0) uniform sampler2D envmap;
layout(binding=7, set=0, rgba8) uniform image2D poolImage;

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

// functions
float stepAndOutputRNGFloat(inout uint rngState)
{
  // Condensed version of pcg_output_rxs_m_xs_32_32, with simple conversion to floating-point [0,1].
  rngState  = rngState * 747796405 + 1;
  uint word = ((rngState >> ((rngState >> 28) + 4)) ^ rngState) * 277803737;
  word      = (word >> 22) ^ word;
  return float(word) / 4294967295.0f;
}

// Generate a random unsigned int from two unsigned int values, using 16 pairs
// of rounds of the Tiny Encryption Algorithm. See Zafar, Olano, and Curtis,
// "GPU Random Numbers via the Tiny Encryption Algorithm"
uint tea16(uint val0, uint val1)
{
  uint v0 = val0;
  uint v1 = val1;
  uint s0 = 0;

  [[unroll]]
  for(uint n = 0; n < 16; n++)
  {
    s0 += 0x9e3779b9;
    v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
    v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
  }

  return v0;
}

uint tea8(uint val0, uint val1)
{
  uint v0 = val0;
  uint v1 = val1;
  uint s0 = 0;

  [[unroll]]
  for(uint n = 0; n < 8; n++)
  {
    s0 += 0x9e3779b9;
    v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
    v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
  }

  return v0;
}

vec3 randomUnitVector(inout uint randState) 
{
  float a = stepAndOutputRNGFloat(randState) * 2. * M_PI;
  float z = stepAndOutputRNGFloat(randState) * 2. - 1.;
  float r = sqrt(1 - z * z);
  return vec3(r * cos(a), r * sin(a), z);
}

vec3 randomCosDirection(inout uint randState)
{
  const float r1 = stepAndOutputRNGFloat(randState);
  const float r2 = stepAndOutputRNGFloat(randState);
  const float r2Sqrt = sqrt(r2);
  const float z = sqrt(1 - r2);
  const float phi = M_PI2 * r1;

  return vec3(cos(phi) * r2Sqrt, sin(phi) * r2Sqrt, z);
}

vec3 setFaceNormal(const vec3 rayDir, const vec3 outwardNormal)
{
  return dot(rayDir, outwardNormal) < 0 ? outwardNormal : -outwardNormal;
}

float schlick(float cosine, float ref_idx) 
{
  float r0 = (1 - ref_idx) / (1 + ref_idx);
  r0 = r0 * r0;
  return r0 + (1 - r0) * pow((1 - cosine), 5);
}

float coef(int x)
{
    float numer = 1.;
    float denom = 1.;

    while(x > 1)
    {
        denom *= x--;
        numer *= x--;
    }

    return numer / denom;
}

float t_cdf(const float t, const int DoF)
{
    const float theta = (t > 0 ? 1. : -1.) * atan(sqrt(t * t / DoF));
    const float c = cos(theta);
    const float s = sin(theta);

    float p = 0;
    if (DoF % 2 == 1)
    {
        p += theta;

        for (int i = 1; i <= DoF - 2; i += 2)
        {
            p += coef(i) * s * pow(c, i);
        }
        
        p /= M_PI;
    }
    else
    {
        p += 1.;
        
        for (int i = 2; i <= DoF - 2; i += 2)
        {
            p += coef(i) * pow(c, i);
        }
        
        p *= s / 2.;
    }

    return 0.5 + p;
}

vec3 correctNaN(const vec3 v)
{
  if (v.x != v.x || v.y != v.y || v.z != v.z)
    return vec3(0.0);
  return v;
}

vec3 local(ONB onb, vec3 pos)
{
  return pos.x * onb.u + pos.y * onb.v + pos.z * onb.w;
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