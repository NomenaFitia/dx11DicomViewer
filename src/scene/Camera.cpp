#include "Camera.h"
#include <algorithm>

using namespace DirectX;

void OrbitCamera::setPerspective(float fovY, float aspect, float znear, float zfar)
{
    mode_ = ProjectionMode::Perspective;
    fovY_ = fovY; aspect_ = aspect; zn_ = znear; zf_ = zfar;
    rebuildProjection();
}

void OrbitCamera::setOrthographic(float orthoHeight, float aspect, float znear, float zfar)
{
    mode_ = ProjectionMode::Orthographic;
    orthoHeight_ = std::max(1e-4f, orthoHeight);
    aspect_ = aspect; zn_ = znear; zf_ = zfar;
    rebuildProjection();
}

void OrbitCamera::setAspect(float aspect)
{
    aspect_ = std::max(1e-6f, aspect);
    rebuildProjection();
}

void OrbitCamera::setZRange(float zn, float zf)
{
    zn_ = zn; zf_ = zf;
    rebuildProjection();
}

void OrbitCamera::rebuildProjection()
{
    if (mode_ == ProjectionMode::Perspective) {
        XMStoreFloat4x4(&mProj, XMMatrixPerspectiveFovLH(fovY_, aspect_, zn_, zf_));
    }
    else {
        // Ortho centrée : largeur = hauteur * aspect
        const float w = orthoHeight_ * aspect_;
        const float h = orthoHeight_;
        XMStoreFloat4x4(&mProj, XMMatrixOrthographicLH(w, h, zn_, zf_));
        // Variante équivalente :
        // XMStoreFloat4x4(&mProj, XMMatrixOrthographicOffCenterLH(-w*0.5f, w*0.5f, -h*0.5f, h*0.5f, zn_, zf_));
    }
}

void OrbitCamera::arc(float dyaw, float dpitch)
{
    yaw += dyaw; pitch += dpitch;
    pitch = std::clamp(pitch, -1.4f, 1.4f);
}

void OrbitCamera::dolly(float d)
{
    if (mode_ == ProjectionMode::Perspective) {
        distance = std::clamp(distance * (1.0f - d), 0.1f, 10000.0f);
    }
    else {
        // En ortho : le "zoom" change l’échelle de la fenêtre de vue
        orthoHeight_ = std::clamp(orthoHeight_ * (1.0f - d), 1e-3f, 1e6f);
        rebuildProjection();
    }
}

void OrbitCamera::pan(float dx, float dy)
{
    XMVECTOR f = XMVectorSet(cosf(yaw) * cosf(pitch), sinf(pitch), sinf(yaw) * cosf(pitch), 0);
    XMVECTOR r = XMVector3Normalize(XMVector3Cross(f, XMVectorSet(0, 1, 0, 0)));
    XMVECTOR u = XMVector3Normalize(XMVector3Cross(r, f));
    XMVECTOR dt = XMVectorAdd(XMVectorScale(r, -dx), XMVectorScale(u, dy));
    XMFLOAT3 t; XMStoreFloat3(&t, XMVectorAdd(XMLoadFloat3(&target), dt));
    target = t;
}

XMMATRIX OrbitCamera::view() const
{
    XMVECTOR t = XMLoadFloat3(&target);
    XMVECTOR dir = XMVectorSet(cosf(yaw) * cosf(pitch), sinf(pitch), sinf(yaw) * cosf(pitch), 0);
    XMVECTOR eye = XMVectorAdd(t, XMVectorScale(dir, -distance));
    return XMMatrixLookAtLH(eye, t, XMVectorSet(0, 1, 0, 0));
}

XMFLOAT3 OrbitCamera::eyePosition() const
{
    XMVECTOR t = XMLoadFloat3(&target);
    XMVECTOR dir = XMVectorSet(cosf(yaw) * cosf(pitch), sinf(pitch), sinf(yaw) * cosf(pitch), 0);
    XMVECTOR eye = XMVectorAdd(t, XMVectorScale(dir, -distance));
    XMFLOAT3 e; XMStoreFloat3(&e, eye); return e;
}
