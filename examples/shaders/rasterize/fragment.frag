#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "../common/types.glsl"
#include "../common/constants.glsl"

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec4 outColor;

layout(binding = 0, set = 0) uniform SceneUB {
    mat4 model;
    mat4 view;
    mat4 proj;
} sceneUB;


layout(binding = 1, set = 0) uniform UniformBufferObject {
    mat4 model;
    uint matIndex;
    vec3 padding;
} instanceUB;

layout(binding=2, set=0) readonly buffer Materials { Material materials[]; };
layout(binding=3, set=0) uniform sampler2D texSamplers[];

void main()
{
    Material material = materials[instanceUB.matIndex];

    if (instanceUB.matIndex % 3 == 0)
    {
        outColor = vec4(RED, 1.0);
    }
    else if (instanceUB.matIndex % 3 == 1)
    {
        outColor = vec4(GREEN, 1.0);
    }
    else
    {
        outColor = vec4(BLUE, 1.0);
    }

    //outColor = vec4(material.albedo, 1.0);
    //outColor = vec4(material.emissive);

    // vec4 albedo = material.albedo;

    // if (inNormal == vec3(0, 0, 0))
    // {
    //     outColor = vec4(1.0, 0, 0, 0);
    //     return;
    // }

    // vec4 ambientColor = vec4(0.1, 0.1, 0.1, 1.0);
    // vec4 lightDirection = vec4(1.0, 1.0, 1.0, 0.0);
    // vec4 eyeDirection = vec4(2.0, 2.0, 2.0, 0.0);

    // vec3  invLight  = normalize(lightDirection).xyz;
    // vec3  invEye    = normalize(eyeDirection).xyz;
    // vec3  halfLE    = normalize(invLight + invEye);
    // float diffuse   = clamp(dot(inNormal, invLight), 0.0, 1.0);
    // float specular  = pow(clamp(dot(inNormal, halfLE), 0.0, 1.0), 30.0);
    // vec4  destColor = albedo * vec4(vec3(diffuse), 1.0) + vec4(vec3(specular), 1.0);// + ambientColor;

    // outColor = destColor;
}