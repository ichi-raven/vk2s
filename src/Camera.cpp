#include "../include/vk2s/Camera.hpp"

#include <glm/gtc/matrix_transform.hpp>

constexpr double kPI = 3.14159265358979;

inline const double sgn(double x)
{
    return x >= 0 ? 1. : -1.;
}

namespace vk2s
{
    Camera::Camera(const double fov, const double aspect, const double near, const double far)
        : mFOV(glm::radians(fov))
        , mAspect(aspect)
        , mNear(near)
        , mFar(far)
        , mPos(0., 0., 0.)
        , mUp(0., 0., 1.)
        , mMoved(false)
    {
        setLookAt(glm::vec3(0.f, 0.f, 1.f));

        updateViewMat();
        updateProjMat();
    }

    void Camera::update(GLFWwindow* pWindow, const double moveSpeed, const double mouseSpeed)
    {
        mMoved = false;

        if (glfwGetMouseButton(pWindow, GLFW_MOUSE_BUTTON_RIGHT))
        {
            double mx = 0, my = 0;
            int width = 0, height = 0;
            glfwGetCursorPos(pWindow, &mx, &my);
            glfwGetWindowSize(pWindow, &width, &height);
            glfwSetCursorPos(pWindow, width / 2, height / 2);
            mPhi += mouseSpeed * static_cast<double>(1. * width / 2. - mx);
            mTheta += mouseSpeed * static_cast<double>(my - 1. * height / 2.);

            if (mTheta > 89.f) mTheta = 89.f;
            if (mTheta < -89.f) mTheta = -89.f;

            mMoved = true;
        }

        glm::vec3 direction(cos(glm::radians(mTheta)) * sin(glm::radians(mPhi)), sin(glm::radians(mTheta)), cos(glm::radians(mTheta)) * cos(glm::radians(mPhi)));
        glm::vec3 right = glm::normalize(glm::cross(glm::vec3(0.f, 1.f, 0.f), direction));  //(sin(mPhi - kPI / 2.), 0, cos(mPhi - kPI / 2.));
        mUp             = glm::normalize(glm::cross(right, direction));

        if (glfwGetKey(pWindow, GLFW_KEY_R))
        {
            mTheta = 0.;
            mPhi   = 0.;
            mMoved = true;
        }

        double moveSpeedMod = moveSpeed;

        if (glfwGetKey(pWindow, GLFW_KEY_LEFT_CONTROL))
        {
            moveSpeedMod *= 10.;
        }

        if (glfwGetKey(pWindow, GLFW_KEY_W))
        {
            mPos += static_cast<float>(moveSpeedMod) * direction;
            mMoved = true;
        }
        if (glfwGetKey(pWindow, GLFW_KEY_S))
        {
            mPos -= static_cast<float>(moveSpeedMod) * direction;
            mMoved = true;
        }
        if (glfwGetKey(pWindow, GLFW_KEY_A))
        {
            mPos -= static_cast<float>(moveSpeedMod) * right;
            mMoved = true;
        }
        if (glfwGetKey(pWindow, GLFW_KEY_D))
        {
            mPos += static_cast<float>(moveSpeedMod) * right;
            mMoved = true;
        }
        if (glfwGetKey(pWindow, GLFW_KEY_UP))
        {
            mPos -= static_cast<float>(moveSpeedMod) * mUp;
            mMoved = true;
        }
        if (glfwGetKey(pWindow, GLFW_KEY_DOWN))
        {
            mPos += static_cast<float>(moveSpeedMod) * mUp;
            mMoved = true;
        }

        updateViewMat();
        updateProjMat();
    }

    void Camera::setPos(const glm::vec3& pos)
    {
        mPos = pos;
        updateViewMat();
        mMoved = true;
    }

    const glm::vec3& Camera::getPos() const
    {
        return mPos;
    }

    void Camera::setLookAt(const glm::vec3& lookAt)
    {
        const auto diff = glm::normalize(lookAt - mPos);

        mPhi = glm::degrees(acos(diff.z / sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z)));

        if (diff.x == 0. && diff.y == 0.)
        {
            mTheta = 0.;
        }
        else
        {
            mTheta = glm::degrees(sgn(diff.y) * acos(diff.x / sqrt(diff.x * diff.x + diff.y * diff.y)));
        }

        updateViewMat();
    }

    const glm::vec3 Camera::getLookAt() const
    {
        return mPos + glm::vec3(cos(glm::radians(mTheta)) * sin(glm::radians(mPhi)), sin(glm::radians(mTheta)), cos(glm::radians(mTheta)) * cos(glm::radians(mPhi)));
    }

    void Camera::setFOV(const double fov)
    {
        mFOV = fov;
        updateProjMat();
    }

    double Camera::getFOV() const
    {
        return mFOV;
    }

    void Camera::setAspect(const double aspect)
    {
        mAspect = aspect;
        updateProjMat();
    }

    double Camera::getAspect() const
    {
        return mAspect;
    }

    void Camera::setNear(const double near)
    {
        mNear = near;
        updateProjMat();
    }

    double Camera::getNear() const
    {
        return mNear;
    }

    void Camera::setFar(const double far)
    {
        mFar = far;
        updateProjMat();
    }

    double Camera::getFar() const
    {
        return mFar;
    }

    bool Camera::moved() const
    {
        return mMoved;
    }

    void Camera::updateViewMat()
    {
        mViewMat = glm::lookAt(mPos, getLookAt(), mUp);
    }

    void Camera::updateProjMat()
    {
        mProjectionMat = glm::perspective(mFOV, mAspect, mNear, mFar);
    }

    const glm::mat4& Camera::getViewMatrix() const
    {
        return mViewMat;
    }

    const glm::mat4& Camera::getProjectionMatrix() const
    {
        return mProjectionMat;
    }

}  // namespace vk2s