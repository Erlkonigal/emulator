#pragma once

#include "emulator/cpu/cpu.h"
#include "emulator/generated/hardware_config.h"
#include "emulator/utils/ring_queue.h"
#include "emulator/utils/singleton.h"

class CommitQueue : public RingQueue<CommitInfo>,
                    public Singleton<CommitQueue> {
public:
  explicit CommitQueue(size_t capacity = kCommitQueueSize)
      : RingQueue<CommitInfo>(capacity) {}
  friend class Singleton<CommitQueue>;
};