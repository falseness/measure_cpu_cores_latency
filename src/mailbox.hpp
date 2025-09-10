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

inline void WriteTimestamp(Mailbox* mailbox, uint64_t tsc) {
  std::memcpy(mailbox->line0, &tsc, sizeof(tsc));
}

inline uint64_t ReadTimestamp(const Mailbox* mailbox) {
  uint64_t tsc = 0;
  std::memcpy(&tsc, mailbox->line0, sizeof(tsc));
  return tsc;
}

inline void MutateSecondLine(Mailbox* mailbox, uint64_t seed) {
  auto* q = reinterpret_cast<uint64_t*>(mailbox->line1);
  for (std::size_t i = 0; i < kQwordsPerLine; ++i) {
    q[i] = seed + static_cast<uint64_t>(i);
  }
}

inline void TouchSecondLine(const Mailbox* mailbox) {
  const volatile uint64_t* q = reinterpret_cast<const volatile uint64_t*>(mailbox->line1);
  uint64_t sink = 0;
  for (std::size_t i = 0; i < kQwordsPerLine; ++i) {
    sink ^= q[i];
  }
  asm volatile("" :: "r"(sink));
}

}  // namespace icore
