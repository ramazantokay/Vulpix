#pragma once
#include "../Math/Vulpix_Math.h"

// Quaternion based camera

struct Viewport
{
	int left;
	int top;
	int right;
	int bottom;
};

class Camera
{
public:
	Camera();
	~Camera();
	
	//setters
	void setPosition(const vulpix::math::vec3& position);
	void setFOV(float fov);
	void setPlanes(float nearPlane, float farPlane);
	void setViewport(const Viewport& viewport);
	void setAspectRatio();

	void moveCamera(const float side, const float direction);
	void rotateCamera(const float x, const float y);
	void lookCameraAt(const vulpix::math::vec3& position, const vulpix::math::vec3& target);


	//getters
	const vulpix::math::vec3& getPosition() const { return m_position; }
	const vulpix::math::vec3& getForward() const {return m_forward;}
	const vulpix::math::vec3 getUp() const { return vulpix::math::vec3(m_viewMatrix[0][1], m_viewMatrix[1][1], m_viewMatrix[2][1]); }
	const vulpix::math::vec3 getSide() const { return vulpix::math::vec3(m_viewMatrix[0][0], m_viewMatrix[1][0], m_viewMatrix[2][0]); }

	const vulpix::math::mat4& getViewMatrix() const { return m_viewMatrix; }
	const vulpix::math::mat4& getProjectionMatrix() const { return m_projectionMatrix; }

	float getFOV() const { return m_fov; }
	float getNearPlane() const { return m_nearPlane; }
	float getFarPlane() const { return m_farPlane; }
	float getAspectRatio() const { return m_aspectRatio; }


private:
	void updateViewMatrix();
	void updateProjectionMatrix();

private:
	vulpix::math::vec3 m_position;
	vulpix::math::vec3 m_forward;
	vulpix::math::vec3 m_up;
	vulpix::math::quat m_rotation;

	float m_fov;
	float m_nearPlane;
	float m_farPlane;
	float m_aspectRatio;

	vulpix::math::mat4 m_viewMatrix; // transform matrix
	vulpix::math::mat4 m_projectionMatrix; // projection matrix

	Viewport m_viewport;

};