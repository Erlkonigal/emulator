#pragma once

#include "emulator/cpu/cpu.h"
#include "emulator/utils/ring_queue.h"
#include "emulator/utils/singleton.h"

class CommitQueue : public RingQueue<CommitInfo>,
                    public Singleton<CommitQueue> {
public:
  explicit CommitQueue(size_t capacity = kDefaultCommitQueueSize)
      : RingQueue<CommitInfo>(capacity) {}
  friend class Singleton<CommitQueue>;
};