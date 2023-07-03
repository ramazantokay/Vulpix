#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : require

#include "../../Shader/Shader_Config.h"

layout(set = VULPIX_SCENE_AS_SET,     binding = VULPIX_SCENE_AS_BINDING)            uniform accelerationStructureEXT Scene;
layout(set = VULPIX_RESULT_IMAGE_SET, binding = VULPIX_RESULT_IMAGE_BINDING, rgba8) uniform image2D ResultImage;

layout(set = VULPIX_CAMDATA_SET,      binding = VULPIX_CAMDATA_BINDING, std140)     uniform AppData {
    UniformParams Params;
};

layout(location = VULPIX_PRIMARY_RAYGEN_SHADER_LOC) rayPayloadEXT RayPayLoad PrimaryRay;
layout(location = VULPIX_SHADOW_RAYGEN_SHADER_LOC)  rayPayloadEXT ShadowRayPayLoad ShadowRay;

const float eratoRefract = 1.0f / 1.31f; // ice refract

vec3 CalcRayDir(vec2 screenUV, float aspect) {
    vec3 u = Params.m_cameraRight.xyz;
    vec3 v = Params.m_cameraUp.xyz;

    const float planeWidth = tan(Params.m_cameraNearFarFOV.z * 0.5f);

    u *= (planeWidth * aspect);
    v *= planeWidth;

    const vec3 rayDir = normalize(Params.m_cameraDirection.xyz + (u * screenUV.x) - (v * screenUV.y));
    return rayDir;
}

void main() {
    const vec2 curPixel = vec2(gl_LaunchIDEXT.xy);
    const vec2 bottomRight = vec2(gl_LaunchSizeEXT.xy - 1);

    const vec2 uv = (curPixel / bottomRight) * 2.0f - 1.0f;

    const float aspect = float(gl_LaunchSizeEXT.x) / float(gl_LaunchSizeEXT.y);

    vec3 origin = Params.m_cameraPosition.xyz;
    vec3 direction = CalcRayDir(uv, aspect);

    const uint rayFlags = gl_RayFlagsOpaqueEXT;
    const uint shadowRayFlags = gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT;

    const uint cullMask = 0xFF;

    const uint stbRecordStride = 1;

    const float tmin = 0.0f;
    const float tmax = Params.m_cameraNearFarFOV.y;

    vec3 finalColor = vec3(0.0f);

    for (int i = 0; i < VULPIX_MAX_RECURSION; ++i) {
        traceRayEXT(Scene,
                    rayFlags,
                    cullMask,
                    VULPIX_PRIMARY_HIT_SHADERS_INDEX,
                    stbRecordStride,
                    VULPIX_PRIMARY_MISS_SHADERS_INDEX,
                    origin,
                    tmin,
                    direction,
                    tmax,
                    VULPIX_PRIMARY_RAYGEN_SHADER_LOC);

        const vec3 hitColor = PrimaryRay.m_colorAndDistance.rgb;
        const float hitDistance = PrimaryRay.m_colorAndDistance.w;

        // if hit background - quit
        if (hitDistance < 0.0f) {
            finalColor += hitColor;
            break;
        } else {
            const vec3 hitNormal = PrimaryRay.m_normalAndObjectId.xyz;
            const float objectId = PrimaryRay.m_normalAndObjectId.w;

            const vec3 hitPos = origin + direction * hitDistance;

            if (objectId == VULPIX_OBJECT_ID_TEAPOT) {
                // reflection part

                origin = hitPos + hitNormal * 0.001f;
                direction = reflect(direction, hitNormal);
            } else if (objectId == VULPIX_OBJECT_ID_ERATO) {
                //  refract part

                const float NdotD = dot(hitNormal, direction);

                vec3 refrNormal = hitNormal;
                float refrEta;

                if(NdotD > 0.0f) {
                    refrNormal = -hitNormal;
                    refrEta = 1.0f / eratoRefract;
                } else {
                    refrNormal = hitNormal;
                    refrEta = eratoRefract;
                }

                origin = hitPos + direction * 0.001f;
                direction = refract(direction, refrNormal, refrEta);
            } else {
                // diffuse part

                const vec3 toLight = normalize(Params.m_sunPosAndAmbient.xyz);
                const vec3 shadowRayOrigin = hitPos + hitNormal * 0.001f;

                traceRayEXT(Scene,
                            shadowRayFlags,
                            cullMask,
                            VULPIX_PRIMARY_SHADOW_HIT_SHADERS_INDEX,
                            stbRecordStride,
                            VULPIX_PRIMARY_SHADOW_MISS_SHADERS_INDEX,
                            shadowRayOrigin,
                            0.0f,
                            toLight,
                            tmax,
                            VULPIX_SHADOW_RAYGEN_SHADER_LOC);

                const float lighting = (ShadowRay.m_distance > 0.0f) ? Params.m_sunPosAndAmbient.w : max(Params.m_sunPosAndAmbient.w, dot(hitNormal, toLight));

                finalColor += hitColor * lighting;

                break;
            }
        }
    }

    imageStore(ResultImage, ivec2(gl_LaunchIDEXT.xy), vec4(linearToSrgb(finalColor), 1.0f));
}
