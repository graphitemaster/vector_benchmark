#include <stddef.h>    // size_t
#include <stdlib.h>    // realloc, free
#include <stdio.h>     // printf
#include <string.h>    // memset
#include <time.h>      // clock_t, clock

#include <utility>     // std::move, std::exchange, std::forward
#include <type_traits> // std::is_trivially_{copyable,constructible,destructible}_v
#include <vector>      // std::vector

#if defined(_MSC_VER)
#define force_inline __force_inline
#else
#define force_inline __attribute__((always_inline)) inline
#endif

// custom placement-new so it can be forced inlined
struct placement_new {};
force_inline constexpr void* operator new(size_t, void* ptr, placement_new) noexcept { return ptr; }

template<typename T>
struct Array {
  using value_type = T;
  constexpr Array() = default;
  ~Array() { clear(); free(m_data); }
  Array(Array&& array)
    : m_data{std::exchange(array.m_data, nullptr)}
    , m_size{std::exchange(array.m_size, 0)}
    , m_capacity{std::exchange(array.m_capacity, 0)}
  {}
  template<typename... Ts>
  force_inline bool emplace_back(Ts&&... args) noexcept {
    if (!ensure(size() + 1)) return false;
    new (m_data + m_size, placement_new{}) T{std::forward<Ts>(args)...};
    m_size++;
    return true;
  }
  force_inline bool push_back(T&& value) noexcept {
    if (!ensure(size() + 1)) return false;
    new (m_data + m_size, placement_new{}) T{std::move(value)};
    m_size++;
    return true;
  }
  force_inline bool push_back(const T& value) noexcept {
    if (!ensure(size() + 1)) return false;
    new (m_data + m_size, placement_new{}) T{value};
    m_size++;
    return true;
  }
  force_inline void clear() noexcept {
    // Rely on unsigned underflow to walk in reverse order for calling destructors
    if constexpr (!std::is_trivially_destructible_v<T>) {
      if (m_size) for (size_t i = m_size - 1; i < m_size; i--) m_data[i].~T();
    }
    m_size = 0;
  }
  bool resize(size_t size) noexcept {
    if (size <= m_size) {
      if constexpr(!std::is_trivially_destructible_v<T>) {
        if (m_size) for (size_t i = m_size - 1; i < size; i++) m_data[i].~T();
      }
    } else {
      if (!ensure(size)) return false;
      if constexpr (std::is_trivially_constructible_v<T>) {
        memset(m_data + m_size, 0, (size - m_size) * sizeof(T));
      } else {
        for (size_t i = m_size; i < size; i++) new (m_data + i, placement_new{}) T;
      }
    }
    m_size = size;
    return true;
  }
  force_inline T* begin() noexcept { return m_data; }
  force_inline const T* begin() const noexcept { return m_data; }
  force_inline T* end() noexcept { return m_data + m_size; }
  force_inline const T* end() const noexcept { return m_data + m_size; }
  force_inline const T& operator[](size_t index) const noexcept { return m_data[index]; }
  force_inline T& operator[](size_t index) noexcept { return m_data[index]; }
  force_inline bool empty() const noexcept { return m_size == 0; }
  force_inline size_t size() const noexcept { return m_size; }
  force_inline size_t capacity() const noexcept { return m_capacity; }
  force_inline const T* data() const noexcept { return m_data; }
  force_inline T* data() noexcept{ return m_data; }
private:
  bool ensure(size_t size) noexcept {
    if (size <= m_capacity) return true;
    size_t new_capacity = m_capacity;
    while (new_capacity < size) new_capacity = ((new_capacity + 1) * 3) / 2;
    if (sizeof(T) > (size_t)-1/new_capacity) return false; // sizeof(T) * new_capacity overflow.
    void *data = nullptr;
    if constexpr (std::is_trivially_copyable_v<T>) {
      data = realloc(m_data, sizeof(T) * new_capacity);
    } else {
      data = malloc(sizeof(T) * new_capacity);
    }
    if (!data) return false; // out of memory
    if constexpr (!std::is_trivially_copyable_v<T>) {
      if (m_size) {
        auto store = reinterpret_cast<T*>(data);
        for (auto item = begin(), last = end(); item != last; ++item) {
          *store++ = std::move(*item);
          item->~T();
        }
      }
      free(m_data);
    }
    m_data = reinterpret_cast<T*>(data);
    m_capacity = new_capacity;
    return true;
  }
  T* m_data = nullptr;
  size_t m_size = 0;
  size_t m_capacity = 0;
};

// Simple benchmark timer.
struct Timer {
  clock_t t0, t1;
  void start() noexcept { t0 = clock(); }
  void stop() noexcept { t1 = clock(); }
  double elapsed() const noexcept {
    return (double)(t1 - t0) / CLOCKS_PER_SEC;
  }
};

// Some types to test the Array implementation with.
struct NonTrivial {
  NonTrivial() {}
  NonTrivial(NonTrivial&&) {}
  ~NonTrivial() {}
  NonTrivial(const NonTrivial&) {}
  NonTrivial& operator=(NonTrivial&&) { return *this; }
  NonTrivial& operator=(const NonTrivial&) { return *this; }
  char buffer[128] = {0};
};
struct Trivial { float x = 0.0f, y = 1.0f, z = 2.0f; };
using POD = size_t;

static constexpr const auto ITERATIONS = 5'000'000;

template<typename T>
T test() noexcept {
  srand(0xdeadbeef); // constant seed to be fair.
  T array;
  Timer timer;
  timer.start();
  typename T::value_type to_copy{};
  for (size_t i = 0; i < ITERATIONS; i++) {
    array.push_back(to_copy);
    typename T::value_type to_move{};
    array.push_back(std::move(to_move));
    array.emplace_back(); // to_forward
  }
  timer.stop();
  printf("%f\t", timer.elapsed());
  return array;
}

#ifndef OPTION
#define OPTION ""
#endif

int main() {
  printf("\"array %s\"  ", OPTION);
  auto x0 = test<Array<POD>>();
  auto x1 = test<Array<Trivial>>();
  auto x2 = test<Array<NonTrivial>>();
  printf("\n");
  printf("\"vector %s\" ", OPTION);
  auto y0 = test<std::vector<POD>>();
  auto y1 = test<std::vector<Trivial>>();
  auto y2 = test<std::vector<NonTrivial>>();
  printf("\n");
}