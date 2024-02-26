/*****************************************************************//**
 * @file   SlotMap.hpp
 * @brief  template definition of class SlotMap
 * 
 * @author ichi-raven
 * @date   November 2023
 *********************************************************************/

#ifndef SLOTMAP_HPP_
#define SLOTMAP_HPP_

#include <cstdint>
#include <cstddef>
#include <vector>
#include <cassert>

// you can change this but you shouldn't
using IDType = std::uint64_t;
// the invalid value of handles
constexpr IDType kInvalidID = 0xFFFFFFFFFFFFFFFF;
// default page allcoation size
constexpr std::size_t kDefaultPageSize = 32;
// default allocator Type
using DefaultAllocator = std::allocator<std::byte>;

template <IDType N, typename = std::is_integral<IDType>::type>
struct IsPowerOf2
{
    static constexpr bool value = (((N - 1U) & N) == 0) && N > 0;
};

template <typename T, std::size_t PageSize, typename Allocator, typename IsPowerOf2>
class Pool;

template <typename T, std::size_t PageSize = kDefaultPageSize, typename Allocator = DefaultAllocator>
class Handle
{
protected:
    using PoolType = Pool<T, PageSize, Allocator, std::enable_if_t<IsPowerOf2<PageSize>::value>>;
    friend PoolType;

public:
    Handle()
        : mID(kInvalidID)
        , mpPool(nullptr)
    {
    }

    virtual ~Handle()
    {
    }

    Handle(const Handle& other)
    {
        this->mID    = other.mID;
        this->mpPool = other.mpPool;
    }

    Handle& operator=(const Handle& other)
    {
        this->mID    = other.mID;
        this->mpPool = other.mpPool;

        return *this;
    }

    Handle(Handle&& other)
    {
        this->mID    = other.mID;
        this->mpPool = other.mpPool;

        other.mID = kInvalidID;
    }

    Handle& operator=(Handle&& other)
    {
        this->mID    = other.mID;
        this->mpPool = other.mpPool;

        other.mID = kInvalidID;

        return *this;
    }

    IDType getRawID() const
    {
        return mID;
    }

    explicit operator bool() const noexcept
    {
        return mID != kInvalidID;
    }

    bool operator!() const noexcept
    {
        return !static_cast<bool>(*this);
    }

    T& get()
    {
        return mpPool->get(*this);
    }

    T* operator->() const
    {
        return &mpPool->get(*this);
    }

protected:
    Handle(IDType id, PoolType* pPool)
        : mID(id)
        , mpPool(pPool)
    {
    }

    // ID(Nbit) = activeFlag(1bit) | slot(N / 2 - 1bit) | index(N / 2bit)
    IDType mID;
    PoolType* mpPool;
};

template <typename T, std::size_t PageSize = kDefaultPageSize, typename Allocator = DefaultAllocator>
class UniqueHandle : public Handle<T, PageSize, Allocator>
{
    using HandleType = Handle<T, PageSize, Allocator>;

public:
    UniqueHandle()
        : HandleType()
    {
    }

    virtual ~UniqueHandle()
    {
        if (this->mID != kInvalidID)
        {
            this->mpPool->deallocate(*this);
            this->mID = kInvalidID;
        }
    }

    UniqueHandle(const UniqueHandle& other)      = delete;
    UniqueHandle& operator=(UniqueHandle& other) = delete;

    UniqueHandle(HandleType&& other)
        : HandleType(std::move(other))
    {
    }

    UniqueHandle& operator=(HandleType&& other)
    {
        HandleType::operator=(std::move(other));
        return *this;
    }

protected:
    UniqueHandle(IDType id, HandleType::PoolType* pPool)
        : HandleType(id, pPool)
    {
    }
};

template <typename T, std::size_t PageSize = kDefaultPageSize, typename Allocator = DefaultAllocator, typename = std::enable_if_t<IsPowerOf2<PageSize>::value>>
class Pool
{
    using HandleType       = Handle<T, PageSize, Allocator>;
    using UniqueHandleType = UniqueHandle<T, PageSize, Allocator>;

public:
    Pool()
    {
    }

    ~Pool()
    {
        clear();
    }

    Pool(const Pool<T, PageSize>& other)                         = delete;
    Pool<T, PageSize>& operator=(const Pool<T, PageSize>& other) = delete;

    Pool(Pool<T, PageSize>&& other)                         = delete;
    Pool<T, PageSize>& operator=(Pool<T, PageSize>&& other) = delete;

    template <typename... Args>
    HandleType allocate(Args&&... args)
    {
        return HandleType(allocInternal<Args...>(std::forward<Args>(args)...), this);
    }

    bool deallocate(HandleType& handle)
    {
        const bool res = deallocInternal(handle.getRawID());
        
        handle.mID = kInvalidID;
        return res;
    }

    T& get(HandleType handle)
    {
        return getInternal(handle.getRawID());
    }

    // if you have a few handles, you should use get() directly
    template <typename Function>
    void forEach(Function function)
    {
        for (auto [pPage, activeHandleNum] : mPageTable)
        {
            // skip empty page
            if (activeHandleNum == 0)
            {
                continue;
            }

            const IDType* IDptr    = reinterpret_cast<IDType*>(pPage + TypeOffset);
            T* objectPtr           = reinterpret_cast<T*>(pPage);
            constexpr uint64_t MSB = (static_cast<IDType>(1) << (ShiftLength * 2 - 1));
            for (std::size_t i = 0; i < PageSize; ++i)
            {
                if (IDptr[i] & MSB)  // if active
                {
                    function(objectPtr[i]);
                }
            }
        }
    }

    void clear()
    {
        forEach([](T& val) { val.~T(); });

        for (auto [pPage, activeHandleNum] : mPageTable)
        {
            mAllocator.deallocate(pPage, PageByteSize);
        }

        mPageTable.clear();
        mFreePosTable.clear();
    }

private:
    template <typename... Args>
    IDType allocInternal(Args... args)
    {
        if (mFreePosTable.empty())
        {
            std::byte* pPage = mAllocator.allocate(PageByteSize);

            for (std::size_t i = 0; i < PageSize; ++i)
            {
                IDType id                                                           = mPageTable.size() * PageSize + i;
                *reinterpret_cast<IDType*>(pPage + TypeOffset + sizeof(IDType) * i) = id;
                mFreePosTable.emplace_back(id);
            }

            mPageTable.emplace_back(pPage, 0);
        }

        // register ID
        const auto id = mFreePosTable.back();
        mFreePosTable.pop_back();

        //constexpr uint32_t MSB = getMSB(PageSize);
        const auto div = id / PageSize;  // id >> MSB;
        const auto mod = id % PageSize;  // (id << (ShiftLength - MSB)) >> (ShiftLength - MSB);

        // increment active handle num in page
        ++mPageTable[div].second;

        // construct
        {
            T* constructPtr = reinterpret_cast<T*>(mPageTable[div].first + sizeof(T) * mod);
            new (constructPtr) T(std::forward<Args>(args)...);
        }

        auto pTargetID = reinterpret_cast<IDType*>(mPageTable[div].first + TypeOffset + sizeof(IDType) * mod);
        // activate flag
        *pTargetID |= (static_cast<IDType>(1) << (ShiftLength * 2 - 1));

        return *pTargetID;
    }

    bool deallocInternal(const IDType id)
    {
        const auto maskedID = id & Mask;
        const auto div      = maskedID / PageSize;
        const auto mod      = maskedID % PageSize;

        assert(div < mPageTable.size() || !"invalid handle!");

        // destruct
        {
            T* ptr = reinterpret_cast<T*>(mPageTable[div].first + sizeof(T) * mod);
            ptr->~T();
        }

        // rotate slot
        {
            auto pObjectID = reinterpret_cast<IDType*>(mPageTable[div].first + TypeOffset + sizeof(IDType) * mod);

            *pObjectID = (*pObjectID & Mask) | (((*pObjectID >> ShiftLength) + 1) << ShiftLength);

            // deactivate flag
            *pObjectID &= ~(static_cast<IDType>(1) << (ShiftLength * 2 - 1));
        }

        // remove active ID and return to be free
        mFreePosTable.emplace_back(maskedID);

        // decrement active page handle num
        --mPageTable[div].second;

        return true;
    }

    T& getInternal(const IDType id)
    {
        assert(id != kInvalidID || !"invalid handle!");

        const uint32_t maskedID = static_cast<uint32_t>(id & Mask);
        //constexpr uint32_t MSB = getMSB(PageSize);
        const auto div = maskedID / PageSize;  // maskedID >> MSB;
        const auto mod = maskedID % PageSize;  // (maskedID << (32 - MSB)) >> (32 - MSB);

        assert(div < mPageTable.size() || !"invalid handle!");
        assert(id == *reinterpret_cast<IDType*>(mPageTable[div].first + TypeOffset + sizeof(IDType) * mod) || !"invalid handle!");

        return *reinterpret_cast<T*>(mPageTable[div].first + sizeof(T) * mod);
    }

    constexpr static std::size_t TypeOffset   = PageSize * sizeof(T);
    constexpr static std::size_t PageByteSize = TypeOffset + PageSize * sizeof(IDType);
    constexpr static std::size_t ShiftLength  = sizeof(IDType) * 4;  // * 8 / 2
    constexpr static IDType Mask              = (static_cast<IDType>(1) << ShiftLength) - 1;

    Allocator mAllocator;
    std::vector<std::pair<std::byte*, std::uint32_t>> mPageTable;
    std::vector<IDType> mFreePosTable;
};

#endif