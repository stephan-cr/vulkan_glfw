#include "allocator.hpp"

#include "catch2/catch_test_macros.hpp"

#include <cstddef>
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
  using Allocator_ = AlignedAllocator<int, unsigned>;
  Allocator_ a;

  int *p = a.allocate(10);
  REQUIRE(p != nullptr);
  REQUIRE(reinterpret_cast<std::size_t>(p) % sizeof(Allocator_::align_as_type) == 0);
  a.deallocate(p, 10);

  REQUIRE(std::allocator_traits<Allocator_>::max_size(a) > 0);
  int *p2 = std::allocator_traits<Allocator_>::allocate(a, 23);
  REQUIRE(p2 != nullptr);
  std::allocator_traits<Allocator_>::deallocate(a, p2, 23);
}

TEST_CASE("allocator implements allocator traits correctly", "[allocator]")
{
  STATIC_REQUIRE(
                 std::is_same<
                 std::allocator_traits<AlignedAllocator<int, unsigned>>::value_type,
                 AlignedAllocator<int, unsigned>::value_type>::value);

  STATIC_REQUIRE(
                 std::is_same<
                 std::allocator_traits<AlignedAllocator<int, unsigned>>::pointer,
                 AlignedAllocator<int, unsigned>::value_type*>::value);
}
