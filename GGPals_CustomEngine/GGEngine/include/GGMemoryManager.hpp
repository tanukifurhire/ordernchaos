/*************************************************************************
  \file       GGMemoryManager.hpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       28 January 2026
  \version    2.1
  \brief
    Custom memory manager with memory pool allocator.
  \details
    Pre-allocates large memory pages at startup and divides them into
    fixed-size blocks. Reuses blocks instead of freeing back to OS.
    Automatically adds more pages when needed. Falls back to malloc
    for allocations larger than block size.
  \dependencies
    <cstddef>, <cstdint>, <string>
  \see
    GGECSScriptSystem, ComponentArray
  \history
    - 1.0 (25 January 2026): Initial version. Tracks script system allocations.
    - 1.1 (27 January 2026): Added component array tracking via custom allocator.
    - 2.0 (28 January 2026): Replaced malloc/free with actual memory pool.
                             Pre-allocates 64KB pages divided into 128-byte blocks.
                             Memory is reused instead of freed back to OS.
    - 2.1 (28 January 2026): Fixed compiler warning C4100.
  \copyright
    Copyright (C) 2026 DigiPen Institute of Technology.
*************************************************************************/
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

/*************************************************************************
  \class GGMemoryManager
  \brief
    Singleton memory manager with fixed-size block pool allocator.
  \details
    Pre-allocates 64KB memory pages divided into 128-byte blocks.
    Uses free linked list stored within blocks themselves for fast
    allocation and deallocation. Automatically extends by adding new
    pages when exhausted. Falls back to malloc for oversized allocations.
*************************************************************************/
class GGMemoryManager
{
public:
    /*************************************************************************
      \brief
        Gets singleton instance of memory manager.
      \return
        Reference to the global memory manager instance.
      \details
        Thread-safe initialization via function-local static (C++11).
    *************************************************************************/
    static GGMemoryManager& GetInstance();

    /*************************************************************************
      \brief
        Allocates memory from pre-allocated pool or malloc fallback.
      \param[in] size
        Number of bytes to allocate.
      \param[in] tag
        Category tag for tracking (e.g., "EntityInstance", "ScriptData").
      \return
        Pointer to allocated memory, or nullptr on failure.
      \details
        Returns 128-byte block from free list if size <= 128 bytes.
        Falls back to malloc for oversized allocations (>128 bytes).
        Automatically allocates new 64KB page if free list empty.
    *************************************************************************/
    void* Allocate(size_t size, const char* tag);

    /*************************************************************************
      \brief
        Frees memory back to pool or calls free() for oversized allocations.
      \param[in] ptr
        Pointer to memory block to free.
      \details
        Returns block to free list if from pool. Calls free() for
        oversized allocations that used malloc. Never actually deletes
        pool memory, just marks as available.
    *************************************************************************/
    void Free(void* ptr);

    /*************************************************************************
      \brief
        Prints current memory statistics to console.
      \details
        Displays total allocated, page count, blocks in use, and
        allocation counts by category. Used for proof of functionality.
    *************************************************************************/
    void LogStats() const;

private:
    // Memory pool configuration
    static constexpr size_t BLOCK_SIZE = 128;           // Fixed block size
    static constexpr size_t PAGE_SIZE = 64 * 1024;      // 64KB per page
    static constexpr size_t BLOCKS_PER_PAGE = PAGE_SIZE / BLOCK_SIZE;  // 512 blocks

    /*************************************************************************
      \struct MemoryPage
      \brief
        Represents a pre-allocated 64KB memory page.
      \details
        Pages are linked together. Each page contains 512 blocks of 128 bytes.
    *************************************************************************/
    struct MemoryPage
    {
        char* memory;       // Pointer to 64KB allocation
        MemoryPage* next;   // Next page in linked list

        /*************************************************************************
          \brief
            Allocates a new memory page.
          \param[in] size
            Size of page in bytes (typically 64KB).
        *************************************************************************/
        MemoryPage(size_t size);

        /*************************************************************************
          \brief
            Frees the memory page.
        *************************************************************************/
        ~MemoryPage();
    };

    /*************************************************************************
      \struct FreeBlock
      \brief
        Free block node stored within block itself.
      \details
        Each free block contains pointer to next free block, forming
        intrusive linked list. No extra memory needed for bookkeeping.
    *************************************************************************/
    struct FreeBlock
    {
        FreeBlock* next;    // Next free block
    };

    // Memory pool state
    MemoryPage* pages;              // Head of page list
    FreeBlock* freeList;            // Head of free block list

    // Statistics
    size_t totalAllocated;
    size_t scriptAllocations;
    size_t componentAllocations;
    size_t pageCount;               // Number of 64KB pages allocated
    size_t blocksInUse;             // Current blocks in use

    /*************************************************************************
      \brief
        Allocates a new 64KB page and adds blocks to free list.
      \details
        Called automatically when free list is empty. Divides page into
        512 blocks of 128 bytes each and links them into free list.
    *************************************************************************/
    void AllocateNewPage();

    /*************************************************************************
      \brief
        Initializes free list from a newly allocated page.
      \param[in] pageMemory
        Pointer to start of 64KB page memory.
      \details
        Threads all 512 blocks together into intrusive linked list.
    *************************************************************************/
    void InitializeFreeList(char* pageMemory);

    /*************************************************************************
      \brief
        Checks if pointer is from pool (vs malloc).
      \param[in] ptr
        Pointer to check.
      \return
        True if pointer is within any allocated page.
      \details
        Used by Free() to determine if block should go back to free
        list or be freed via free().
    *************************************************************************/
    bool IsFromPool(void* ptr);

    /*************************************************************************
      \brief
        Private constructor for singleton pattern.
      \details
        Allocates initial 64KB page.
    *************************************************************************/
    GGMemoryManager();

    /*************************************************************************
      \brief
        Private destructor.
      \details
        Frees all memory pages.
    *************************************************************************/
    ~GGMemoryManager();

    // Delete copy and move constructors
    GGMemoryManager(const GGMemoryManager&) = delete;
    GGMemoryManager& operator=(const GGMemoryManager&) = delete;
    GGMemoryManager(GGMemoryManager&&) = delete;
    GGMemoryManager& operator=(GGMemoryManager&&) = delete;
};

/*************************************************************************
  \class PoolAllocator
  \brief
    Custom STL allocator for std::vector to use GGMemoryManager.
  \tparam T
    Type being allocated.
  \details
    Redirects std::vector internal allocations through GGMemoryManager
    instead of using malloc/free directly. Used by ComponentArray to
    track component vector growth.
*************************************************************************/
template<typename T>
class PoolAllocator
{
public:
    using value_type = T;

    /*************************************************************************
      \brief
        Default constructor.
    *************************************************************************/
    PoolAllocator() = default;

    /*************************************************************************
      \brief
        Copy constructor from different type (required by STL).
    *************************************************************************/
    template<typename U>
    PoolAllocator(const PoolAllocator<U>&) {}

    /*************************************************************************
      \brief
        Allocates memory for n objects of type T.
      \param[in] n
        Number of objects to allocate space for.
      \return
        Pointer to allocated memory.
      \details
        Called by std::vector when it needs to grow capacity.
        Uses GGMemoryManager to track allocations.
    *************************************************************************/
    T* allocate(size_t n)
    {
        return static_cast<T*>(
            GGMemoryManager::GetInstance().Allocate(n * sizeof(T), "Component"));
    }

    /*************************************************************************
      \brief
        Frees previously allocated memory.
      \param[in] ptr
        Pointer to memory to free.
      \param[in] n
        Number of objects (unused, for STL compatibility).
      \details
        Delegates to GGMemoryManager for tracked deallocation.
    *************************************************************************/
    void deallocate(T* ptr, [[maybe_unused]] size_t n)
    {
        GGMemoryManager::GetInstance().Free(ptr);
    }
};

/*************************************************************************
  \brief
    Equality comparison for allocators (always true for stateless allocators).
  \details
    Required by STL. Stateless allocators are always equal.
*************************************************************************/
template<typename T, typename U>
bool operator==(const PoolAllocator<T>&, const PoolAllocator<U>&) { return true; }

/*************************************************************************
  \brief
    Inequality comparison for allocators (always false for stateless allocators).
  \details
    Required by STL. Stateless allocators are always equal.
*************************************************************************/
template<typename T, typename U>
bool operator!=(const PoolAllocator<T>&, const PoolAllocator<U>&) { return false; }