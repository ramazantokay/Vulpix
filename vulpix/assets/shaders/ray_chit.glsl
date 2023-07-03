#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "../../Shader/Shader_Config.h"

layout(set = VULPIX_MATERIAL_IDS_SET, binding = 0, std430) readonly buffer MatIDsBuffer {
    uint MatIDs[];
} MatIDsArray[];

layout(set = VULPIX_ATTRIBUTES_SET, binding = 0, std430) readonly buffer AttribsBuffer {
    VertexAttributes VertexAttribs[];
} AttribsArray[];

layout(set = VULPIX_FACES_SET, binding = 0, std430) readonly buffer FacesBuffer {
    uvec4 Faces[];
} FacesArray[];

layout(set = VULPIX_TEXTURES_SET, binding = 0) uniform sampler2D TexturesArray[];

layout(location = VULPIX_PRIMARY_RAYGEN_SHADER_LOC) rayPayloadInEXT RayPayLoad PrimaryRay;
                                                    hitAttributeEXT vec2 HitAttribs;

void main() {
    const vec3 barycentrics = vec3(1.0f - HitAttribs.x - HitAttribs.y, HitAttribs.x, HitAttribs.y);

    const uint matID = MatIDsArray[nonuniformEXT(gl_InstanceCustomIndexEXT)].MatIDs[gl_PrimitiveID];

    const uvec4 face = FacesArray[nonuniformEXT(gl_InstanceCustomIndexEXT)].Faces[gl_PrimitiveID];

    VertexAttributes v0 = AttribsArray[nonuniformEXT(gl_InstanceCustomIndexEXT)].VertexAttribs[int(face.x)];
    VertexAttributes v1 = AttribsArray[nonuniformEXT(gl_InstanceCustomIndexEXT)].VertexAttribs[int(face.y)];
    VertexAttributes v2 = AttribsArray[nonuniformEXT(gl_InstanceCustomIndexEXT)].VertexAttribs[int(face.z)];

    // interpolate our vertex attribs
    const vec3 normal = normalize(barycentricLerp(v0.m_normal.xyz, v1.m_normal.xyz, v2.m_normal.xyz, barycentrics));
    const vec2 uv = barycentricLerp(v0.m_uv.xy, v1.m_uv.xy, v2.m_uv.xy, barycentrics);

    const vec3 texel = textureLod(TexturesArray[nonuniformEXT(matID)], uv, 0.0f).rgb;

    const float objId = float(gl_InstanceCustomIndexEXT);

    PrimaryRay.m_colorAndDistance = vec4(texel, gl_HitTEXT);
    PrimaryRay.m_normalAndObjectId = vec4(normal, objId);
}
