#include "../include/Sampler.hpp"

#include "../include/Device.hpp"

namespace vkpt
{
    Sampler::Sampler(Device& device, const vk::SamplerCreateInfo& ci)
        : mDevice(device)
    {
        mSampler = device.getVkDevice()->createSamplerUnique(ci);
    }

    Sampler::~Sampler()
    {

    }

    const vk::UniqueSampler& Sampler::getVkSampler()
    {
        return mSampler;
    }
}