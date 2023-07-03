#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : require

#include "../../Shader/Shader_Config.h"

layout(location = VULPIX_SHADOW_RAYGEN_SHADER_LOC) rayPayloadInEXT ShadowRayPayLoad ShadowRay;

void main() {
    ShadowRay.m_distance = gl_HitTEXT;
}
