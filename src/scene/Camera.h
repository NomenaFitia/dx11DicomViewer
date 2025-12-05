#pragma once
#include <DirectXMath.h>

enum class ProjectionMode { Perspective, Orthographic };

class OrbitCamera {
public:
	// Vue / position 
	DirectX::XMMATRIX view() const;
	DirectX::XMMATRIX proj() const { return DirectX::XMLoadFloat4x4(&mProj); }
	DirectX::XMFLOAT3 eyePosition() const;

	// Projections
	void setPerspective(float fovY, float aspect, float znear, float zfar);
	void setOrthographic(float orthoHeight, float aspect, float znear, float zfar);
	void setTarget(DirectX::XMFLOAT3 t) { target = t; }

	// Utilitaires communs
	void setAspect(float aspect);      // appeler au resize
	void setZRange(float zn, float zf);
	float aspect() const { return aspect_; }
	ProjectionMode mode() const { return mode_; }

	// Contr�les cam�ra
	void arc(float dyaw, float dpitch);
	void dolly(float d);               // en ortho : zoom = scale de la hauteur visible
	void pan(float dx, float dy);

private:
	void rebuildProjection();

	// etat orbit
	float yaw = 0.f, pitch = 0.f;
	float distance = 5.f;
	DirectX::XMFLOAT3 target{ 0,0,0 };

	// Projection
	ProjectionMode mode_ = ProjectionMode::Perspective;
	float aspect_ = 1.0f;
	float zn_ = 0.1f, zf_ = 1000.0f;

	// Parametres sp�cifiques
	float fovY_ = DirectX::XM_PIDIV4; // perspective
	float orthoHeight_ = 2.0f;        // hauteur visible (monde) en orthographique

	// Stockee en row_major (cf. HLSL)
	DirectX::XMFLOAT4X4 mProj{};
};