#pragma once
#include "../../utility/d3dUtil.h"

class Camera
{
public:
    Camera(float aspect = 1.0f,
        float fovY = DirectX::XM_PIDIV4,
        float nearZ = 1.0f,
        float farZ = 1000.0f,
        DirectX::XMFLOAT3 pos = { 0.f, 5.f, -5.f },
        float yaw = 0.0f,
        float pitch = 0.0f) noexcept;

    Camera(const Camera& rhs) noexcept;
    Camera& operator=(const Camera& rhs) noexcept;

    Camera(Camera&& rhs) noexcept;
    Camera& operator=(Camera&& rhs) noexcept;

    ~Camera() = default;

    void SetLens(float fovY, float aspect, float nearZ, float farZ) noexcept;
    void SetAspect(float aspect) noexcept;
    void OnResize(int width, int height) noexcept;

    void SetPosition(const DirectX::XMFLOAT3& p) noexcept;
    void SetYawPitch(float yaw, float pitch) noexcept;
    void AddYawPitch(float dYaw, float dPitch) noexcept;

    void Walk(float dt) noexcept;
    void Strafe(float dt) noexcept;
    void Rise(float dt) noexcept;

    void Yaw(float radians)   noexcept { AddYawPitch(radians, 0.f); }
    void Pitch(float radians) noexcept { AddYawPitch(0.f, radians); }

    DirectX::XMFLOAT3 GetPositionF() const noexcept { return _position; }
    DirectX::XMVECTOR GetPosition() const noexcept { return DirectX::XMLoadFloat3(&_position); }

    float GetYaw()   const noexcept { return _yaw; }
    float GetPitch() const noexcept { return _pitch; }

    float GetFovY()   const noexcept { return _fovY; }
    float GetAspect() const noexcept { return _aspect; }
    float GetNearZ()  const noexcept { return _nearZ; }
    float GetFarZ()   const noexcept { return _farZ; }

    DirectX::XMMATRIX GetView() const noexcept;
    DirectX::XMMATRIX GetProj() const noexcept;
    DirectX::XMMATRIX GetViewProj() const noexcept;

    DirectX::XMFLOAT3 GetForwardF() const noexcept { return _forward; }
    DirectX::XMFLOAT3 GetRightF()   const noexcept { return _right; }
    DirectX::XMFLOAT3 GetUpF()      const noexcept { return _up; }

    void SetMoveSpeed(float s) noexcept { _moveSpeed = s; }
    float GetMoveSpeed() const noexcept { return _moveSpeed; }

private:
    void UpdateView() const noexcept;

private:
    DirectX::XMFLOAT3 _position{ 0.f, 5.f, -5.f };
    float _yaw = 0.f;
    float _pitch = 0.f;

    mutable DirectX::XMFLOAT3 _forward{ 0.f, 0.f, 1.f };
    mutable DirectX::XMFLOAT3 _right{ 1.f, 0.f, 0.f };
    mutable DirectX::XMFLOAT3 _up{ 0.f, 1.f, 0.f };

    mutable DirectX::XMFLOAT4X4 _view = d3dUtil::Identity4x4();
    DirectX::XMFLOAT4X4 _proj = d3dUtil::Identity4x4();

    float _fovY = DirectX::XM_PIDIV4;
    float _aspect = 1.f;
    float _nearZ = 0.1f;
    float _farZ = 1000.f;

    float _moveSpeed = 25.f;
    float _cameraSensitivty = 0.002f;

    mutable bool _viewDirty = true;
};
