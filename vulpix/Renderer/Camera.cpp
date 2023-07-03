#include "Camera.h"

Camera::Camera()
{
	m_position = vulpix::math::vec3(0.0f, 0.0f, 0.0f);
	m_forward = vulpix::math::vec3(0.0f, 0.0f, 1.0f);
	m_up = vulpix::math::vec3(0.0f, 1.0f, 0.0f);
	m_fov = 45.0f;
	m_nearPlane = 1.0f;
	m_farPlane = 1000.0f;
	m_viewport = { 0, 0, 1920, 1080 };
	m_aspectRatio = static_cast<float>(m_viewport.right - m_viewport.left) / static_cast<float>(m_viewport.bottom - m_viewport.top);

}

Camera::~Camera()
{
}

void Camera::setPosition(const vulpix::math::vec3& position)
{
	m_position = position;
	updateViewMatrix();
}

void Camera::setFOV(float fov)
{
	m_fov = fov;
	updateProjectionMatrix();
}

void Camera::setPlanes(float nearPlane, float farPlane)
{
	m_nearPlane = nearPlane;
	m_farPlane = farPlane;
	updateProjectionMatrix();
}

void Camera::setViewport(const Viewport& viewport)
{
	m_viewport = viewport;
	setAspectRatio();
}

void Camera::setAspectRatio()
{
	m_aspectRatio = static_cast<float>(m_viewport.right - m_viewport.left) / static_cast<float>(m_viewport.bottom - m_viewport.top);
	updateProjectionMatrix();
}

void Camera::moveCamera(const float side, const float direction)
{
	vulpix::math::vec3 right = glm::normalize(glm::cross(m_forward, m_up));
	m_position += right * side;
	m_position += m_forward * direction;
	updateViewMatrix();
}

void Camera::rotateCamera(const float x, const float y)
{
	vulpix::math::vec3 side_vec = glm::cross(m_forward, m_up);
	vulpix::math::quat pitch = glm::angleAxis(glm::radians(y), side_vec);
	vulpix::math::quat yaw = glm::angleAxis(glm::radians(x), m_up);

	vulpix::math::quat rotation = glm::normalize(pitch * yaw);
	m_forward = glm::normalize(glm::rotate(rotation, m_forward));

	updateViewMatrix();
}

void Camera::lookCameraAt(const vulpix::math::vec3& position, const vulpix::math::vec3& target)
{
	m_position = position;
	m_forward = glm::normalize(target - position);
	updateViewMatrix();
}

void Camera::updateViewMatrix() // transform matrix
{
	m_viewMatrix = vulpix::math::matLookAt(m_position, m_position + m_forward, m_up);
}

void Camera::updateProjectionMatrix() // projection
{
	m_projectionMatrix = vulpix::math::matProjection(glm::radians(m_fov), m_aspectRatio, m_nearPlane, m_farPlane);
}
