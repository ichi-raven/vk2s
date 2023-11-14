#ifndef VULKANPLAYGROUND_UTILITY_HPP_
#define VULKANPLAYGROUND_UTILITY_HPP_

#define NONCOPYABLE(TypeName)\
    TypeName(const TypeName& other) = delete;\
    TypeName& operator=(TypeName& other) = delete;

#define NONMOVABLE(TypeName)\
    TypeName(const TypeName&& other) = delete;\
    TypeName&& operator=(TypeName&& other) = delete;

#define VK_FAILED(resultVar, func) const auto resultVar = func; resultVar != vk::eSuccess

#endif