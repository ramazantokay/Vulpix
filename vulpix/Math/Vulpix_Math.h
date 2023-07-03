#ifndef VULPIX_MATH_H
#define VULPIX_MATH_H


#include "../Common.h"

#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

namespace vulpix
{
	namespace math
	{
		using vec2 = glm::highp_vec2;
		using vec3 = glm::highp_vec3;
		using vec4 = glm::highp_vec4;
		using mat4 = glm::highp_mat4;
		using quat = glm::highp_quat;

		constexpr auto matProjection = glm::perspectiveRH_ZO<float>;
		constexpr auto matLookAt = glm::lookAtRH<float, glm::highp>;


	}
} // namespace vulpix

#endif // VULPIX_MATH_H