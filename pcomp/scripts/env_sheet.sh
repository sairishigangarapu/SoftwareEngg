#!/usr/bin/env bash
# Baseline environment sheet -> results/env.md
set -u
OUT="${1:-results/env.md}"; mkdir -p "$(dirname "$OUT")"
{
echo "# Environment sheet ($(date -u +%Y-%m-%dT%H:%M:%SZ))"
echo; echo "## CPU";  (lscpu 2>/dev/null | grep -E 'Model name|^CPU\(s\)|Thread|Core|Socket|MHz|L2|L3') || sysctl -n machdep.cpu.brand_string 2>/dev/null
echo "logical CPUs: $(nproc 2>/dev/null || sysctl -n hw.ncpu)"
echo; echo "## RAM";  (free -h 2>/dev/null | head -2) || sysctl -n hw.memsize 2>/dev/null
echo; echo "## OS";   uname -a; grep PRETTY_NAME /etc/os-release 2>/dev/null
echo; echo "## Compiler"; ${CXX:-c++} --version | head -1; cmake --version | head -1
echo; echo "## Build flags"; grep -m1 '"type":"env"' $(ls -t results/bench_*.jsonl 2>/dev/null | head -1) 2>/dev/null || echo "see CMakeLists.txt (Release: -O2 -DNDEBUG); exact flags are also logged in each results file"
echo; echo "## Governor"; cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || echo "n/a"
} > "$OUT"
echo "wrote $OUT"
