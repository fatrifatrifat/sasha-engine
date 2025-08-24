#include "../../../include/sasha/renderer/scene/Camera.h"
#define max(a,b) (((a) > (b)) ? (a) : (b))
#define min(a,b) (((a) < (b)) ? (a) : (b))
using namespace DirectX;

namespace {
    constexpr float kMinPitch = -XM_PIDIV2 + 0.001f;
    constexpr float kMaxPitch = XM_PIDIV2 - 0.001f;
}

Camera::Camera(float aspect, float fovY, float nearZ, float farZ,
    XMFLOAT3 pos, float yaw, float pitch) noexcept
    : _position(pos), _yaw(yaw), _pitch(pitch),
    _fovY(fovY), _aspect(aspect), _nearZ(nearZ), _farZ(farZ) {
    SetLens(_fovY, _aspect, _nearZ, _farZ);
    _viewDirty = true;
}

Camera::Camera(const Camera& rhs) noexcept
    : _position(rhs._position), _yaw(rhs._yaw), _pitch(rhs._pitch),
    _forward(rhs._forward), _right(rhs._right), _up(rhs._up),
    _view(rhs._view), _proj(rhs._proj),
    _fovY(rhs._fovY), _aspect(rhs._aspect), _nearZ(rhs._nearZ), _farZ(rhs._farZ),
    _moveSpeed(rhs._moveSpeed), _viewDirty(rhs._viewDirty) {}

Camera& Camera::operator=(const Camera& rhs) noexcept {
    if (this == &rhs) return *this;
    _position = rhs._position; _yaw = rhs._yaw; _pitch = rhs._pitch;
    _forward = rhs._forward; _right = rhs._right; _up = rhs._up;
    _view = rhs._view; _proj = rhs._proj;
    _fovY = rhs._fovY; _aspect = rhs._aspect; _nearZ = rhs._nearZ; _farZ = rhs._farZ;
    _moveSpeed = rhs._moveSpeed; _viewDirty = rhs._viewDirty;
    return *this;
}

Camera::Camera(Camera&& rhs) noexcept
    : Camera() {
    *this = std::move(rhs);
}

Camera& Camera::operator=(Camera&& rhs) noexcept {
    if (this == &rhs) return *this;
    _position = rhs._position; rhs._position = { 0,0,0 };
    _yaw = rhs._yaw; rhs._yaw = 0.f;
    _pitch = rhs._pitch; rhs._pitch = 0.f;

    _forward = rhs._forward; _right = rhs._right; _up = rhs._up;
    _view = rhs._view; _proj = rhs._proj;

    _fovY = rhs._fovY; _aspect = rhs._aspect; _nearZ = rhs._nearZ; _farZ = rhs._farZ;
    _moveSpeed = rhs._moveSpeed;
    _viewDirty = rhs._viewDirty;

    return *this;
}

void Camera::SetLens(float fovY, float aspect, float nearZ, float farZ) noexcept {
    _fovY = fovY; _aspect = aspect; _nearZ = nearZ; _farZ = farZ;
    XMStoreFloat4x4(&_proj, XMMatrixPerspectiveFovLH(_fovY, _aspect, _nearZ, _farZ));
}

void Camera::SetAspect(float aspect) noexcept {
    _aspect = aspect;
    SetLens(_fovY, _aspect, _nearZ, _farZ);
}

void Camera::OnResize(int width, int height) noexcept {
    if (height <= 0) height = 1;
    SetAspect(static_cast<float>(width) / static_cast<float>(height));
}

void Camera::SetPosition(const XMFLOAT3& p) noexcept {
    _position = p;
    _viewDirty = true;
}

void Camera::SetYawPitch(float yaw, float pitch) noexcept {
    _yaw = yaw;
    _pitch = max(kMinPitch, min(kMaxPitch, pitch));
    _viewDirty = true;
}

void Camera::AddYawPitch(float dYaw, float dPitch) noexcept {
    SetYawPitch(_yaw + dYaw * _cameraSensitivty, _pitch + dPitch * _cameraSensitivty);
}

void Camera::Walk(float dt) noexcept {
    UpdateView();
    XMVECTOR f = XMLoadFloat3(&_forward);
    XMVECTOR p = XMLoadFloat3(&_position) + f * (_moveSpeed * dt);
    XMStoreFloat3(&_position, p);
    _viewDirty = true;
}

void Camera::Strafe(float dt) noexcept {
    UpdateView();
    XMVECTOR r = XMLoadFloat3(&_right);
    XMVECTOR p = XMLoadFloat3(&_position) + r * (_moveSpeed * dt);
    XMStoreFloat3(&_position, p);
    _viewDirty = true;
}

void Camera::Rise(float dt) noexcept {
    UpdateView();
    XMVECTOR u = XMLoadFloat3(&_up);
    XMVECTOR p = XMLoadFloat3(&_position) + u * (_moveSpeed * dt);
    XMStoreFloat3(&_position, p);
    _viewDirty = true;
}

void Camera::UpdateView() const noexcept {
    if (!_viewDirty) return;

    XMMATRIX R = XMMatrixRotationRollPitchYaw(_pitch, _yaw, 0.f);

    XMVECTOR forward = XMVector3TransformNormal(XMVectorSet(0, 0, 1, 0), R);
    XMVECTOR right = XMVector3TransformNormal(XMVectorSet(1, 0, 0, 0), R);
    XMVECTOR up = XMVector3Normalize(XMVector3Cross(forward, right));

    forward = XMVector3Normalize(forward);
    right = XMVector3Normalize(XMVector3Cross(up, forward));

    XMStoreFloat3(&_forward, forward);
    XMStoreFloat3(&_right, right);
    XMStoreFloat3(&_up, up);

    XMVECTOR pos = XMLoadFloat3(&_position);
    XMMATRIX V = XMMatrixLookToLH(pos, forward, up);
    XMStoreFloat4x4(&_view, V);

    _viewDirty = false;
}

XMMATRIX Camera::GetView() const noexcept {
    UpdateView();
    return XMLoadFloat4x4(&_view);
}

XMMATRIX Camera::GetProj() const noexcept {
    return XMLoadFloat4x4(&_proj);
}

XMMATRIX Camera::GetViewProj() const noexcept {
    UpdateView();
    return XMMatrixMultiply(XMLoadFloat4x4(&_view), XMLoadFloat4x4(&_proj));
}
