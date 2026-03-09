#pragma once

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <new>
#include <stdexcept>
#include <sys/mman.h>
#include <sys/types.h>
#include <type_traits>
#include <unistd.h>

#include "emulator/generated/hardware_config.h"

/**
 * @brief High-performance SPSC Ring Queue using Linux Virtual Memory Magic.
 * * Maps the same physical memory twice into adjacent virtual addresses to
 * eliminate wrap-around checks for contiguous memory access.
 */
#ifndef __linux__
#error "Magic Ring Buffer implementation requires Linux (mmap/memfd_create)"
#endif

template <typename T> class RingQueue {
  // Safety check: T must be POD
  static_assert(std::is_trivial_v<T>, "T must be a trivial type (POD)");
  static_assert(std::is_standard_layout_v<T>, "T must be standard layout");

public:
  explicit RingQueue(size_t minCapacity)
      : mFd(-1), mAddress(nullptr), mBuffer(nullptr) {

    mPageSize = sysconf(_SC_PAGESIZE);

    // 1. Calculate Capacity (Power of 2)
    mCapacity = 1;
    while (mCapacity < minCapacity) {
      mCapacity <<= 1;
    }

    // Ensure byte size is a multiple of page size
    size_t sizeBytes = mCapacity * sizeof(T);
    while (sizeBytes % mPageSize != 0) {
      mCapacity <<= 1;
      sizeBytes = mCapacity * sizeof(T);
    }

    mMask = mCapacity - 1;
    mSizeBytes = sizeBytes;

    try {
      // 2. Create memory-backed file descriptor
      mFd = memfd_create("ring_queue_magic", 0);
      if (mFd == -1) {
        throw std::runtime_error("memfd_create failed");
      }

      if (ftruncate(mFd, mSizeBytes) == -1) {
        throw std::runtime_error("ftruncate failed");
      }

      // 3. Reserve double virtual address space
      mAddress = (uint8_t *)mmap(NULL, 2 * mSizeBytes, PROT_NONE,
                                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

      if (mAddress == MAP_FAILED) {
        mAddress = nullptr;
        throw std::runtime_error("mmap reserve failed");
      }

      // 4. Mirror Mapping (First and Second half)
      void *ptr1 = mmap(mAddress, mSizeBytes, PROT_READ | PROT_WRITE,
                        MAP_SHARED | MAP_FIXED, mFd, 0);
      void *ptr2 = mmap(mAddress + mSizeBytes, mSizeBytes,
                        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, mFd, 0);

      if (ptr1 == MAP_FAILED || ptr2 == MAP_FAILED) {
        throw std::runtime_error("mmap double mapping failed");
      }

      close(mFd);
      mFd = -1;

    } catch (...) {
      cleanup();
      throw;
    }

    mBuffer = reinterpret_cast<T *>(mAddress);
    mHead.store(0);
    mTail.store(0);
    mCachedHead = 0;
    mCachedTail = 0;
  }

  ~RingQueue() { cleanup(); }

  // Deleted Copy/Move
  RingQueue(const RingQueue &) = delete;
  RingQueue &operator=(const RingQueue &) = delete;
  RingQueue(RingQueue &&) = delete;
  RingQueue &operator=(RingQueue &&) = delete;

  /**
   * @brief Request contiguous writing slots.
   */
  [[nodiscard]] T *alloc(size_t count) {
    const size_t currTail = mTail.load(std::memory_order_relaxed);
    const size_t currHead = mCachedHead;
    size_t available = mCapacity - (currTail - currHead);

    if (available < count) {
      const size_t trueHead = mHead.load(std::memory_order_acquire);
      mCachedHead = trueHead;
      available = mCapacity - (currTail - trueHead);
      if (available < count) {
        return nullptr;
      }
    }

    return &mBuffer[currTail & mMask];
  }

  /**
   * @brief Commit the written data.
   */
  void push(size_t count) {
    const size_t currTail = mTail.load(std::memory_order_relaxed);
    mTail.store(currTail + count, std::memory_order_release);
  }

  /**
   * @brief Access contiguous readable data.
   */
  [[nodiscard]] T *front(size_t maxCount, size_t &outCount) {
    const size_t currHead = mHead.load(std::memory_order_relaxed);
    const size_t currTail = mCachedTail;
    size_t available = currTail - currHead;

    if (available == 0) {
      const size_t trueTail = mTail.load(std::memory_order_acquire);
      mCachedTail = trueTail;
      available = trueTail - currHead;
      if (available == 0) {
        outCount = 0;
        return nullptr;
      }
    }

    outCount = std::min(maxCount, available);
    return &mBuffer[currHead & mMask];
  }

  /**
   * @brief Release consumed slots.
   */
  void pop(size_t count) {
    const size_t currHead = mHead.load(std::memory_order_relaxed);
    assert(currHead + count <= mTail.load(std::memory_order_acquire) &&
           "Pop Underflow!");
    mHead.store(currHead + count, std::memory_order_release);
  }

  [[nodiscard]] size_t size() const {
    return mTail.load(std::memory_order_acquire) -
           mHead.load(std::memory_order_acquire);
  }

  [[nodiscard]] size_t capacity() const { return mCapacity; }

  void reset() {
    mHead.store(0, std::memory_order_release);
    mTail.store(0, std::memory_order_release);
    mCachedHead = 0;
    mCachedTail = 0;
  }

private:
  void cleanup() {
    if (mAddress && mAddress != MAP_FAILED) {
      munmap(mAddress, 2 * mSizeBytes);
      mAddress = nullptr;
    }
    if (mFd != -1) {
      close(mFd);
      mFd = -1;
    }
  }

  int mFd;
  size_t mPageSize;
  size_t mSizeBytes;
  uint8_t *mAddress;

  T *mBuffer;
  size_t mCapacity;
  size_t mMask;

  alignas(kPadding) std::atomic<size_t> mHead{0};
  size_t mCachedTail{0};

  alignas(kPadding) std::atomic<size_t> mTail{0};
  size_t mCachedHead{0};
};