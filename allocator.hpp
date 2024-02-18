#ifndef ALLOCATOR_HPP
#define ALLOCATOR_HPP

#include <cstddef>
#include <cstdlib>
#include <limits>
#include <new>

//! models an Allocator https://en.cppreference.com/w/cpp/named_req/Allocator
template<typename T, typename AlignAsT>
class AlignedAllocator
{
public:
  using value_type = T;

  [[nodiscard]] T* allocate(std::size_t n)
  {
    if (n > std::numeric_limits<std::size_t>::max() / sizeof(T))
      throw std::bad_array_new_length();

    auto* p = static_cast<T*>(std::aligned_alloc(alignof(AlignAsT), sizeof(T) * n));
    if (p == nullptr)
      throw std::bad_alloc();

    return p;
  }

  void deallocate(T* p, std::size_t n) noexcept
  {
    std::free(p);
  }
};

template<typename T, typename U, typename AlignAsT>
bool operator==(const AlignedAllocator<T, AlignAsT>&, const AlignedAllocator<U, AlignAsT>&) { return true; }

template<typename T, typename U, typename AlignAsT>
bool operator!=(const AlignedAllocator<T, AlignAsT>&, const AlignedAllocator<U, AlignAsT>&) { return false; }

#endif // ALLOCATOR_HPP
