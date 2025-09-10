#pragma once


#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>


struct alignas(128) Mailbox {
alignas(64) std::atomic<uint64_t> seq{0};
alignas(64) std::atomic<uint64_t> ack{0};
alignas(64) unsigned char line0[64];
alignas(64) unsigned char line1[64];
};


inline void WriteTimestamp(Mailbox* m, uint64_t t) {
std::memcpy(m->line0, &t, sizeof(t));
}


inline uint64_t ReadTimestamp(const Mailbox* m) {
uint64_t t;
std::memcpy(&t, m->line0, sizeof(t));
return t;
}


inline void MutateSecondLine(Mailbox* m, uint64_t seed) {
auto* p = reinterpret_cast<uint64_t*>(m->line1);
for (int i = 0; i < 8; ++i) p[i] = seed + static_cast<uint64_t>(i);
}


inline void TouchSecondLine(const Mailbox* m) {
volatile const uint64_t* p = reinterpret_cast<const uint64_t*>(m->line1);
uint64_t s = 0;
for (int i = 0; i < 8; ++i) s ^= p[i];
asm volatile("" :: "r"(s));
}