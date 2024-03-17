#include "allocator.hpp"

#include "catch2/catch_test_macros.hpp"

#include <memory>
#include <type_traits>

TEST_CASE("allocators are equal", "[allocator]")
{
  AlignedAllocator<int, unsigned> a1;
  AlignedAllocator<int, unsigned> a2;

  REQUIRE(a1 == a2);
}

TEST_CASE("allocator is able to allocate memory", "[allocator]")
{
  AlignedAllocator<int, unsigned> a;

  int *p = a.allocate(10);
  a.deallocate(p, 10);

  using Allocator_ = AlignedAllocator<int, unsigned>;
  int *p2 = std::allocator_traits<Allocator_>::allocate(a, 23);
  std::allocator_traits<Allocator_>::deallocate(a, p2, 23);
}

static_assert(
  std::is_same<
    std::allocator_traits<AlignedAllocator<int, unsigned>>::value_type,
    AlignedAllocator<int, unsigned>::value_type>::value,
  "value_type must be set");

static_assert(
  std::is_same<
    std::allocator_traits<AlignedAllocator<int, unsigned>>::pointer,
    AlignedAllocator<int, unsigned>::value_type*>::value,
  "pointer_type must be set");
