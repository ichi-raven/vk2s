/*****************************************************************/ /**
 * @file   Camera.hpp
 * @brief  Camera utility class
 * 
 * @author ichi-raven
 * @date   October 2023
 *********************************************************************/
#ifndef VK2S_CAMERA_HPP_
#define VK2S_CAMERA_HPP_

#include <glm/glm.hpp>

#include <GLFW/glfw3.h>

namespace vk2s
{
class Camera
{
public:
   
    Camera(const double fov = 60., const double aspect = 16. / 9., const double near = 0.1, const double far = 1e4);

    void update(GLFWwindow* pWindow, const double moveSpeed, const double mouseSpeed, const bool reset = false);

    void setPos(const glm::vec3& pos);
    const glm::vec3& getPos() const;

    void setLookAt(const glm::vec3& lookAt);
    const glm::vec3 getLookAt() const;

    void setFOV(const double FOV);
    double getFOV() const;

    void setAspect(const double aspect);
    double getAspect() const;

    void setNear(const double near);
    double getNear() const;

    void setFar(const double far);
    double getFar() const;

    bool moved() const;

    const glm::mat4& getViewMatrix() const;
    const glm::mat4& getProjectionMatrix() const;

private:
    void updateViewMat();
    void updateProjMat();

    double mFOV;
    double mAspect;
    double mNear;
    double mFar;
    double mTheta;
    double mPhi;
    glm::vec3 mPos;
    glm::vec3 mUp;
    glm::mat4 mViewMat;
    glm::mat4 mProjectionMat;

    bool mMoved;
};
}
#endif
