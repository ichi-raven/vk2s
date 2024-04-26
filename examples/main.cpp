#include <iostream>

// stb implementation
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

void pathtracing(const uint32_t windowWidth, const uint32_t windowHeight, const uint32_t frameCount);

void rasterize(const uint32_t windowWidth, const uint32_t windowHeight, const uint32_t frameCount);

int main()
{
    constexpr uint32_t kWidth = 1000;
    constexpr uint32_t kHeight = 1000;
    constexpr uint32_t kFrameCount = 3;

    //rasterize(kWidth, kHeight, kFrameCount);
    
    pathtracing(kWidth, kHeight, kFrameCount);

    return 0;
}