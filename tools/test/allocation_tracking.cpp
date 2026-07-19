// Allocation tracking implementation
// This file contains the actual definitions of the global tracking variables
// and the overridden malloc/free/new/delete operators.

#include "allocation_tracking.h"

#if defined(_WIN32) && defined(_DEBUG)
  #include <crtdbg.h>
#endif

// Allocation tracking globals - definitions
namespace allocation_tracking
{
volatile int g_allocation_count = 0;
volatile int g_deallocation_count = 0;
volatile bool g_tracking_enabled = false;

// Original malloc/free functions
#ifndef _WIN32
void* (*original_malloc)(size_t) = nullptr;
void (*original_free)(void*) = nullptr;
void* (*original_realloc)(void*, size_t) = nullptr;
#endif

#if defined(_WIN32) && defined(_DEBUG)
namespace
{
int __cdecl allocation_hook(int allocation_type, void*, size_t, int, long, const unsigned char*, int)
{
  if (!allocation_tracking::g_tracking_enabled)
    return 1;

  if (allocation_type == _HOOK_ALLOC)
    ++allocation_tracking::g_allocation_count;
  else if (allocation_type == _HOOK_FREE)
    ++allocation_tracking::g_deallocation_count;
  else if (allocation_type == _HOOK_REALLOC)
  {
    ++allocation_tracking::g_allocation_count;
    ++allocation_tracking::g_deallocation_count;
  }

  return 1;
}
} // namespace
#endif

void prepare_tracking()
{
#if defined(_WIN32) && defined(_DEBUG)
  static const bool installed = []() {
    _CrtSetAllocHook(allocation_hook);
    return true;
  }();
  (void)installed;
#endif
}
} // namespace allocation_tracking

// Override malloc/free to track Eigen allocations (Eigen uses malloc directly)
#ifndef _WIN32
extern "C" {
void* malloc(size_t size)
{
  if (!allocation_tracking::original_malloc)
    allocation_tracking::original_malloc = reinterpret_cast<void* (*)(size_t)>(dlsym(RTLD_NEXT, "malloc"));
  void* ptr = allocation_tracking::original_malloc(size);
  if (allocation_tracking::g_tracking_enabled && ptr != nullptr)
    ++allocation_tracking::g_allocation_count;
  return ptr;
}

void free(void* ptr)
{
  if (!allocation_tracking::original_free)
    allocation_tracking::original_free = reinterpret_cast<void (*)(void*)>(dlsym(RTLD_NEXT, "free"));
  if (allocation_tracking::g_tracking_enabled && ptr != nullptr)
    ++allocation_tracking::g_deallocation_count;
  allocation_tracking::original_free(ptr);
}

void* realloc(void* ptr, size_t size)
{
  if (!allocation_tracking::original_realloc)
    allocation_tracking::original_realloc = reinterpret_cast<void* (*)(void*, size_t)>(dlsym(RTLD_NEXT, "realloc"));
  void* new_ptr = allocation_tracking::original_realloc(ptr, size);
  if (allocation_tracking::g_tracking_enabled)
  {
    if (ptr != nullptr && new_ptr != ptr)
      ++allocation_tracking::g_deallocation_count; // Old pointer was freed
    if (new_ptr != nullptr && new_ptr != ptr)
      ++allocation_tracking::g_allocation_count; // New allocation
  }
  return new_ptr;
}
}
#endif

// Overload global new/delete operators to track allocations
void* operator new(std::size_t size)
{
  void* ptr = std::malloc(size);
  if (!ptr)
    throw std::bad_alloc();
  if (allocation_tracking::g_tracking_enabled)
    ++allocation_tracking::g_allocation_count;
  return ptr;
}

void* operator new[](std::size_t size)
{
  void* ptr = std::malloc(size);
  if (!ptr)
    throw std::bad_alloc();
  if (allocation_tracking::g_tracking_enabled)
    ++allocation_tracking::g_allocation_count;
  return ptr;
}

void operator delete(void* ptr) noexcept
{
  if (allocation_tracking::g_tracking_enabled && ptr != nullptr)
    ++allocation_tracking::g_deallocation_count;
  std::free(ptr);
}

void operator delete[](void* ptr) noexcept
{
  if (allocation_tracking::g_tracking_enabled && ptr != nullptr)
    ++allocation_tracking::g_deallocation_count;
  std::free(ptr);
}
