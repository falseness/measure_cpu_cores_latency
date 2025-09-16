#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace icore {

constexpr std::size_t kCacheLineBytes = 64;
constexpr std::size_t kMailboxAlignBytes = 2 * kCacheLineBytes;
constexpr std::size_t kQwordsPerLine = kCacheLineBytes / sizeof(uint64_t);

struct alignas(kMailboxAlignBytes) Mailbox {
  alignas(kCacheLineBytes) std::atomic<uint64_t> seq{0};
  alignas(kCacheLineBytes) std::atomic<uint64_t> ack{0};
  alignas(kCacheLineBytes) unsigned char line0[kCacheLineBytes];
  alignas(kCacheLineBytes) unsigned char line1[kCacheLineBytes];
};

inline void WriteTimestampAndFirstLine(Mailbox* mailbox, uint64_t tsc, uint64_t seed) {
  std::memcpy(mailbox->line0, &tsc, sizeof(tsc));
  for (std::size_t i = sizeof(tsc); i < kCacheLineBytes; ++i) {
    mailbox->line0[i] = seed + i;
  }
}

inline uint64_t ReadTimestampAndFirstLine(const Mailbox* mailbox) {
  uint64_t tsc = 0;
  std::memcpy(&tsc, mailbox->line0, sizeof(tsc));
  uint64_t sink = 0;
  for (size_t i = sizeof(tsc); i < kCacheLineBytes; ++i) {
    sink += mailbox->line0[i];
  }
  asm volatile("" :: "r"(sink));
  return tsc;
}

inline void WriteSecondLine(Mailbox* mailbox, unsigned char seed) {
  for (size_t i = 0; i < kCacheLineBytes; ++i) {
    mailbox->line1[i] = seed + i;
  }
}

inline void ReadSecondLine(const Mailbox* mailbox) {
  uint64_t sink = 0;
  for (size_t i = 0; i < kCacheLineBytes; ++i) {
    sink += mailbox->line1[i];
  }
  asm volatile("" :: "r"(sink));
}

}  // namespace icore
