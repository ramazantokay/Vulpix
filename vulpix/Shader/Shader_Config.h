#ifndef VULPIX_SHADER_CONFIG_H
#define VULPIX_SHADER_CONFIG_H

#ifdef __cplusplus
#include "../Common.h"
#include "../Math/Vulpix_Math.h"
using namespace vulpix::math;
#endif

#define VULPIX_PRIMARY_HIT_SHADERS_INDEX                    0
#define VULPIX_PRIMARY_MISS_SHADERS_INDEX                   0
#define VULPIX_PRIMARY_SHADOW_HIT_SHADERS_INDEX             1
#define VULPIX_PRIMARY_SHADOW_MISS_SHADERS_INDEX            1

#define VULPIX_MATERIAL_IDS_SET                             1
#define VULPIX_ATTRIBUTES_SET                               2
#define VULPIX_FACES_SET                                    3
#define VULPIX_TEXTURES_SET                                 4
#define VULPIX_ENVIRONMENT_MAP_SET                          5

// resourse locations
#define VULPIX_SCENE_AS_SET                                 0
#define VULPIX_SCENE_AS_BINDING                             0
#define VULPIX_RESULT_IMAGE_SET                             0
#define VULPIX_RESULT_IMAGE_BINDING                         1
#define VULPIX_CAMDATA_SET                                  0   
#define VULPIX_CAMDATA_BINDING                              2

// shader locs
#define VULPIX_PRIMARY_RAYGEN_SHADER_LOC                    0
#define VULPIX_HIT_ATTRIBUTES_SHADER_LOC                    1
#define VULPIX_SHADOW_RAYGEN_SHADER_LOC                     2


//#define VULPIX_OBJECT_ID_BUNNY                              0.0f
//#define VULPIX_OBJECT_ID_PLANE                              1.0f
//#define VULPIX_OBJECT_ID_TEAPOT                             2.0f
//#define VULPIX_OBJECT_ID_ERATO                              3.0f
//#define VULPIX_OBJECT_ID_DRAGON                             4.0f

#define VULPIX_OBJECT_ID_TEAPOT                             2.0f
#define VULPIX_OBJECT_ID_ERATO                              1.0f
#define VULPIX_OBJECT_ID_SPONZA                             0.0f


// config vars
#define VULPIX_MAX_RECURSION 10 // bounce count

// shader structs

struct RayPayLoad
{
    vec4 m_colorAndDistance;
    vec4 m_normalAndObjectId;
};

struct ShadowRayPayLoad
{
	float m_distance;
};

struct VertexAttributes
{
	vec4 m_normal;
	vec4 m_uv;
};

struct UniformParams
{
    vec4 m_cameraPosition;
    vec4 m_cameraDirection;
    vec4 m_cameraUp;
    vec4 m_cameraRight;
    vec4 m_cameraNearFarFOV;
    vec4 m_sunPosAndAmbient;

};

// shader helper functions, it is intersting that we can use c++ functions in shaders :D (her gun yeni bi bilgi :P sasirmaya devamke)

vec2 barycentricLerp(vec2 a, vec2 b, vec2 c, vec3 barycentric)
{
	return a * barycentric.x + b * barycentric.y + c * barycentric.z;
}

vec3 barycentricLerp(vec3 a, vec3 b, vec3 c, vec3 barycentric)
{
	return a * barycentric.x + b * barycentric.y + c * barycentric.z;
}


float linearToSrgb(float channel) {
    if (channel <= 0.0031308f) {
        return 12.92f * channel;
    }
    else {
        return 1.055f * pow(channel, 1.0f / 2.4f) - 0.055f;
    }
}

vec3 linearToSrgb(vec3 linear) {
    return vec3(linearToSrgb(linear.r), linearToSrgb(linear.g), linearToSrgb(linear.b));
}

#endif // VULPIX_SHADER_CONFIG_H