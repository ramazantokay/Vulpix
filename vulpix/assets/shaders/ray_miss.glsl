#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : require

#include "../../Shader/Shader_Config.h" 

layout(set = VULPIX_ENVIRONMENT_MAP_SET, binding = 0) uniform sampler2D EnvTexture;

layout(location = VULPIX_PRIMARY_RAYGEN_SHADER_LOC) rayPayloadInEXT RayPayLoad PrimaryRay;

const float MY_PI = 3.1415926535897932384626433832795;
const float MY_INV_PI  = 1.0 / MY_PI;

vec2 DirToLatLong(vec3 dir) {
    float phi = atan(dir.x, dir.z);
    float theta = acos(dir.y);

    return vec2((MY_PI + phi) * (0.5 / MY_PI), theta * MY_INV_PI);
}

void main() {
    vec2 uv = DirToLatLong(gl_WorldRayDirectionEXT);
    vec3 envColor = textureLod(EnvTexture, uv, 0.0).rgb;
    PrimaryRay.m_colorAndDistance = vec4(envColor, -1.0);
    PrimaryRay.m_normalAndObjectId = vec4(0.0);
}
