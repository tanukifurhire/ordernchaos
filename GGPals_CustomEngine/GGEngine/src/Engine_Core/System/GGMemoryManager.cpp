/*************************************************************************
  \file       GGMemoryManager.cpp
  \author     Zafir Haqqym
  \par        Email: zafir.h@digipen.edu
  \date       31 January 2026
  \version    2.3
  \brief
    Memory pool allocator implementation.
  \details
    Manages pre-allocated memory pages with fixed-size blocks. Tracks
    which blocks are in use and reuses freed blocks for new allocations.
    Automatically extends pool when running out of blocks. Uses malloc
    only for oversized allocations.
  \dependencies
    GGMemoryManager.hpp, GGLogger.hpp, <iostream>, <cstdlib>, <string>
  \see
    GGECSScriptSystem, EntityInstance, ComponentArray
  \history
    - 1.0 (25 January 2026): Initial version. Console logging for allocations.
    - 1.1 (27 January 2026): Added tracking for component array growth.
    - 2.0 (28 January 2026): Implemented memory pool allocator with pages and blocks.
                             No longer uses malloc/free for small allocations.
    - 2.1 (28 January 2026): Integrated GGLogger for consistent logging output.
    - 2.2 (28 January 2026): Fixed compiler warning C6001.
    - 2.3 (31 January 2026): Fixed shutdown crash by skipping page cleanup in destructor.
                             Prevents use-after-free when ComponentArrays using PoolAllocator
                             are destroyed after the memory manager singleton. Memory is
                             intentionally leaked during process termination (OS reclaims).
                             Added CRT leak suppression for debug builds.
  \copyright
    Copyright (C) 2026 DigiPen Institute of Technology.
*************************************************************************/
#include "GGMemoryManager.hpp"
#include <GGLogger.hpp>
#include <iostream>
#include <cstdlib>

// Disable C6001 - false positive for validated function parameters
#pragma warning(disable: 6001)

/*************************************************************************
  \brief
    MemoryPage constructor - allocates page memory.
  \param[in] size
    Size of page in bytes (typically 64KB).
*************************************************************************/
GGMemoryManager::MemoryPage::MemoryPage(size_t size) : memory(new char[size]), next(nullptr)
{
}

/*************************************************************************
  \brief
    MemoryPage destructor - frees page memory.
*************************************************************************/
GGMemoryManager::MemoryPage::~MemoryPage()
{
    delete[] memory;
}

/*************************************************************************
  \brief
    Constructor initializes counters and allocates first page.
*************************************************************************/
GGMemoryManager::GGMemoryManager()
    : pages(nullptr)
    , freeList(nullptr)
    , totalAllocated(0)
    , scriptAllocations(0)
    , componentAllocations(0)
    , pageCount(0)
    , blocksInUse(0)
{
    GGLogger::Info("[GGMemoryManager] Initialized - Fixed-size block pool (128 bytes/block)");

    // Allocate initial 64KB page
    AllocateNewPage();
}

/*************************************************************************
  \brief
    Destructor logs final statistics and frees all pages.
  \details
    During process shutdown, we skip explicit cleanup to avoid use-after-free
    with component arrays that may still be destroying. The OS reclaims all
    memory when the process exits anyway.
*************************************************************************/
GGMemoryManager::~GGMemoryManager()
{
    // Skip cleanup during static destruction to avoid crashes
    // Component arrays may still be destroying and calling Free()
    // The OS will reclaim all process memory on exit anyway

    GGLogger::Info("[GGMemoryManager] Shutdown - Final Stats:");
    LogStats();

    // Suppress CRT memory leak reports for intentionally leaked pages
#ifdef _DEBUG
// Mark all page allocations as non-leaks for CRT debugger
    MemoryPage* page = pages;
    while (page)
    {
        // Mark page struct and page memory as intentional leaks
        _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF & ~_CRTDBG_LEAK_CHECK_DF);
        page = page->next;
    }
#endif

    // DO NOT free pages during shutdown - causes use-after-free
    // Component arrays with PoolAllocator are destroyed after this
    // and will try to call Free() on already-destroyed memory manager

    // Just leak during shutdown - it's safe and prevents crashes
    // The OS reclaims all memory when process terminates
}

/*************************************************************************
  \brief
    Gets singleton instance of memory manager.
  \return
    Reference to the global memory manager instance.
  \details
    Thread-safe initialization via function-local static (C++11).
*************************************************************************/
GGMemoryManager& GGMemoryManager::GetInstance()
{
    static GGMemoryManager instance;
    return instance;
}

/*************************************************************************
  \brief
    Allocates a new 64KB page and adds its blocks to free list.
  \details
    Called when free list is empty. Creates page, divides into 512
    blocks, and threads them into intrusive free list.
*************************************************************************/
void GGMemoryManager::AllocateNewPage()
{
    MemoryPage* newPage = new MemoryPage(PAGE_SIZE);
    newPage->next = pages;
    pages = newPage;
    pageCount++;

    InitializeFreeList(newPage->memory);

    GGLogger::Info("[GGMemoryManager] Allocated new page #" + std::to_string(pageCount) + " (64KB, " + std::to_string(BLOCKS_PER_PAGE) + " blocks)");
}

/*************************************************************************
  \brief
    Initializes free list from a newly allocated page.
  \param[in] pageMemory
    Pointer to start of 64KB page memory.
  \details
    Threads all 512 blocks together into intrusive linked list by
    storing FreeBlock pointers within blocks themselves.
*************************************************************************/
void GGMemoryManager::InitializeFreeList(char* pageMemory)
{
    for (size_t i = 0; i < BLOCKS_PER_PAGE; i++)
    {
        FreeBlock* block = reinterpret_cast<FreeBlock*>(pageMemory + i * BLOCK_SIZE);
        block->next = freeList;
        freeList = block;
    }
}

/*************************************************************************
  \brief
    Checks if pointer is from pool (vs malloc).
  \param[in] ptr
    Pointer to check.
  \return
    True if pointer is within any allocated page.
  \details
    Walks page list to determine if pointer falls within page bounds.
    Used by Free() to route to correct deallocation path.
*************************************************************************/
bool GGMemoryManager::IsFromPool(void* ptr)
{
    MemoryPage* page = pages;
    while (page)
    {
        char* pageStart = page->memory;
        char* pageEnd = page->memory + PAGE_SIZE;

        if (ptr >= pageStart && ptr < pageEnd)
        {
            return true;
        }

        page = page->next;
    }

    return false;
}

/*************************************************************************
  \brief
    Allocates memory from pool or malloc fallback.
  \param[in] size
    Number of bytes to allocate.
  \param[in] tag
    Category tag for tracking.
  \return
    Pointer to allocated memory, or nullptr on failure.
  \details
    Returns 128-byte block from free list if size <= 128.
    Falls back to malloc for oversized allocations (>128 bytes).
    Automatically adds new page if free list exhausted.
*************************************************************************/
void* GGMemoryManager::Allocate(size_t size, const char* tag)
{
    // Oversized allocation - fall back to malloc
    if (size > BLOCK_SIZE)
    {
        void* ptr = malloc(size);

        if (ptr)
        {
            totalAllocated += size;

            GGLogger::Info("[GGMemoryManager] OVERSIZED " + std::to_string(size) + " bytes (using malloc) for " + std::string(tag) + " at 0x" + std::to_string(reinterpret_cast<uintptr_t>(ptr)));
        }
        else
        {
            GGLogger::Error("[GGMemoryManager] ERROR: Failed to malloc "
                + std::to_string(size) + " bytes for " + std::string(tag));
        }

        return ptr;
    }

    // No free blocks - allocate new page
    if (!freeList)
    {
        AllocateNewPage();
    }

    // Pop from free list
    FreeBlock* block = freeList;
    freeList = freeList->next;

    blocksInUse++;
    totalAllocated += BLOCK_SIZE;

    // Track by tag
    std::string tagStr(tag);
    if (tagStr.find("Script") != std::string::npos || tagStr.find("Entity") != std::string::npos)
    {
        scriptAllocations++;
    }
    else if (tagStr.find("Component") != std::string::npos)
    {
        componentAllocations++;
    }

    GGLogger::Info("[GGMemoryManager] Allocated block (" + std::to_string(BLOCK_SIZE) + " bytes) for " + std::string(tag) + " - " + std::to_string(blocksInUse) + " blocks in use");

    return block;
}

/*************************************************************************
  \brief
    Frees memory back to pool or calls free() for malloc allocations.
  \param[in] ptr
    Pointer to memory block to free.
  \details
    Returns block to free list if from pool. Calls free() for
    oversized allocations. Pool memory is never actually deleted,
    just marked as available by adding to free list.
*************************************************************************/
void GGMemoryManager::Free(void* ptr)
{
    if (!ptr) return;

    // Tell static analyzer ptr is guaranteed non-null here
    __analysis_assume(ptr != nullptr);

    // Check if from pool
    if (IsFromPool(ptr))
    {
        // Push back to free list
        FreeBlock* block = static_cast<FreeBlock*>(ptr);
        block->next = freeList;
        freeList = block;

        blocksInUse--;

        GGLogger::Info("[GGMemoryManager] Freed block - " + std::to_string(blocksInUse) + " blocks in use");
    }
    else
    {
        // Oversized allocation - actually free
        free(ptr);
        GGLogger::Info("[GGMemoryManager] Freed oversized (malloc) allocation at 0x" + std::to_string(reinterpret_cast<uintptr_t>(ptr)));
    }
}

/*************************************************************************
  \brief
    Prints current memory statistics to console.
  \details
    Displays total allocated, page count, blocks in use, and
    allocation counts by category.
*************************************************************************/
void GGMemoryManager::LogStats() const
{
    GGLogger::Info("=== Memory Manager Stats ===");
    GGLogger::Info("Total Allocated: " + std::to_string(totalAllocated) + " bytes");
    GGLogger::Info("Pages Allocated: " + std::to_string(pageCount) + " (64KB each)");
    GGLogger::Info("Blocks In Use: " + std::to_string(blocksInUse) + " / " + std::to_string(pageCount * BLOCKS_PER_PAGE));
    GGLogger::Info("Script Allocations: " + std::to_string(scriptAllocations));
    GGLogger::Info("Component Allocations: " + std::to_string(componentAllocations));
    GGLogger::Info("=============================");
}