#!/usr/bin/env bash
# Minimal prep for stable inter-core latency runs:
# - set performance governor
# - disable turbo/boost (best-effort)
# - stop irqbalance
# - move movable IRQs off test CPUs

set -euo pipefail

# Edit these for your box
TEST_CPUS="${TEST_CPUS:-6-11}"   # where you'll run the benchmark
ALLOW_CPUS="${ALLOW_CPUS:-0-3}"  # where IRQs are allowed to run

[ "$(id -u)" -eq 0 ] || { echo "Run as root (sudo)"; exit 1; }

echo "[1/4] governor=performance"
for f in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
  [ -w "$f" ] && echo performance > "$f" || true
done

echo "[2/4] disable turbo/boost (best-effort)"
# Intel (intel_pstate)
[ -w /sys/devices/system/cpu/intel_pstate/no_turbo ] && echo 1 > /sys/devices/system/cpu/intel_pstate/no_turbo || true
# Generic/AMD path
[ -w /sys/devices/system/cpu/cpufreq/boost ] && echo 0 > /sys/devices/system/cpu/cpufreq/boost || true

echo "[3/4] stop irqbalance"
systemctl stop irqbalance 2>/dev/null || true

echo "[4/4] move movable IRQs to CPUs: ${ALLOW_CPUS}"
for d in /proc/irq/*; do
  [ -d "$d" ] || continue
  f_list="$d/smp_affinity_list"
  # skip non-writable and per-CPU IRQs
  [ -w "$f_list" ] || continue
  [ -f "$d/per_cpu_devid" ] && continue
  echo "${ALLOW_CPUS}" > "$f_list" 2>/dev/null || true
done