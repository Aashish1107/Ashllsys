# llsys — Low-Latency Trading System

A from-scratch build of the core of an electronic trading system, optimized for latency.

---

## 1. What "low latency" means in trading

The number that matters is **tick-to-trade**: the time from a market-data packet hitting your
network card to your order packet leaving it. It is measured wire-to-wire and reported as
percentiles. **p99 / p99.9 and jitter matter more than the average**: you lose the race on the slow ones,
and the slow ones cluster in busy markets, which is when the money is made.

| Tier                              | Tick-to-trade   | How                                              |
|-----------------------------------|-----------------|--------------------------------------------------|
| FPGA / ASIC                       | ~50 ns – 1 µs   | Decision logic in hardware, on or next to the NIC |
| Tuned software + kernel bypass    | ~1 – 10 µs      | C++ on isolated, pinned cores; NIC read directly from user space |
| Tuned software, normal sockets    | ~10 – 50 µs     | Good place to start                               |
| Typical app / cloud               | milliseconds    | Not competitive for latency-sensitive strategies |

Physics sets the floor: light in fiber covers ~1 km in 5 µs. One kilometre of cable costs more
than all of your code. That is why **colocation** (putting your server in the exchange's
building) comes before any software optimization.

---

## 2. Anatomy of the system

```
                         ┌─────────────────────── HOT PATH (one core per stage, busy-polling) ──────────────────────┐
Exchange ──UDP multicast──▶ Feed handler ──▶ Order book ──▶ Strategy ──▶ Pre-trade risk ──▶ Order gateway ──TCP──▶ Exchange
                         └──────────────┬───────────────────────────────────────────────────────────┬────────────┘
                                        │           lock-free SPSC queues (never block)              │
                                        ▼                                                             ▼
                         OFF HOT PATH: logging · positions & PnL · persistence · monitoring · config · reconciliation
```

**Feed handler.** Exchanges publish market data over UDP multicast in binary protocols:
NASDAQ TotalView-ITCH 5.0 (over MoldUDP64), CME MDP 3.0 (SBE-encoded), NYSE XDP, and so on.
Each feed is sent twice (A and B lines). You take whichever copy arrives first (*arbitration*),
check sequence numbers, detect gaps, and recover from a snapshot or retransmit channel.

**Order book.** Rebuilds the book from those messages: L2 (aggregated price levels) or
L3 (every individual order; ITCH gives you this). Add, modify, cancel and execute must each take
nanoseconds, and best bid/ask must be O(1).

**Strategy.** Market making, arbitrage, and so on. On the hot path it is usually cheap arithmetic
on precomputed parameters. Heavy modelling (ML, regression fitting) runs offline or on a slow
thread that pushes new parameters into the hot thread.

**Pre-trade risk.** Required by law. Max order size and notional, position limits, price collars,
message-rate throttles, duplicate-order checks, and a **kill switch**. These checks are on the hot
path, so they have to run in nanoseconds too.

**Order gateway.** Encodes orders in the venue's native binary protocol (NASDAQ OUCH,
CME iLink 3 / SBE, ...) over TCP, and tracks each order's state from exchange acks and fills.
FIX is the universal standard, but it is text-based and slower, so fast venues offer binary
alternatives.

**Off the hot path.** Logging, persistence, positions and PnL, drop-copy reconciliation,
monitoring and alerting. These are fed through lock-free queues and can never stall the hot path.

---

## 3. Hardware and infrastructure

- **Colocation.** Rack space in the exchange's data center, with a cross-connect to the matching
  engine. In the US: Equinix NY4/NY5 (Secaucus), NYSE (Mahwah), NASDAQ (Carteret), CME (Aurora).
  London, Frankfurt and Tokyo have equivalents.
- **Links between venues.** Microwave and millimetre-wave towers (radio through air is ~1.5×
  faster than light in glass), and hollow-core fiber.
- **NICs with kernel bypass.** AMD Solarflare (Onload, ef_vi, TCPDirect), NVIDIA/Mellanox
  ConnectX (VMA/XLIO, DPDK), Cisco (formerly Exablaze). Your process reads packets straight from
  NIC memory and skips the kernel network stack. The NIC also timestamps packets in hardware.
- **Switches.** Layer-1 switches (Arista 7130, Cisco Nexus 3550) copy packets port-to-port in a
  few ns. Ordinary switches take hundreds of ns.
- **Servers.** The highest single-core clock you can get (often overclocked), a large L3 cache,
  and a single socket so memory never crosses a NUMA link.
- **Time sync.** PTP (IEEE 1588) from a GPS-disciplined grandmaster clock. MiFID II (RTS 25)
  requires HFT clocks to stay within 100 µs of UTC.
- **Capture.** Network taps and capture appliances timestamp every packet in and out, which
  gives you the true wire-to-wire latency.
- **FPGA.** Boards such as AMD Alveo parse the feed and fire the order entirely in hardware.
  This is the last step and the most expensive one in engineering time.

---

## 4. OS tuning (Linux)

Trading systems run on Linux. Each item below removes a source of **jitter** (random stalls):

- **Isolate cores**: kernel parameters `isolcpus=`, `nohz_full=` and `rcu_nocbs=`, then pin each
  hot thread to its own isolated core (`pthread_setaffinity_np`).
- **Interrupts**: steer IRQs away from hot cores (`/proc/irq/*/smp_affinity`) and turn off `irqbalance`.
- **BIOS**: turn off deep C-states and power saving, use the performance profile, and consider
  turning off hyper-threading (the sibling thread competes for cache and execution units).
- **CPU frequency**: use the `performance` governor at a fixed frequency.
- **Memory**: use hugepages (fewer TLB misses), `mlockall()` (no page faults), turn off transparent
  huge pages (compaction stalls), and keep the NIC, memory and hot cores on the same NUMA node.
- **Never sleep, never block**: busy-poll, and make no syscalls on the hot path.
- **Starting baseline**: `tuned-adm profile network-latency`.

---

## 5. Software techniques (C++)

C++ (C++20) is the industry default; Rust is viable. Rules for the hot path:

- **No allocation.** Preallocate everything at startup (pools, arenas, fixed-capacity containers).
  `malloc` can take a lock or trigger a page fault.
- **No locks.** Threads talk through lock-free single-producer/single-consumer ring buffers (the
  LMAX Disruptor idea), and each piece of data has exactly one writer.
- **Cache is king.** An L1 hit costs ~1 ns; a trip to main memory costs ~80–100 ns. Use flat
  arrays, not linked lists or `std::map`. Pad hot shared variables to 64 bytes (`alignas(64)`) to
  avoid false sharing, and keep hot data small.
- **No indirection or surprise branches.** Use templates/CRTP instead of virtual calls, mark
  branches with `[[likely]]` / `[[unlikely]]`, and never throw exceptions on the hot path.
- **Zero-copy parsing.** Read fields directly from the packet buffer at fixed offsets and byte-swap
  the big-endian ones. No string conversion.
- **Fixed-point prices.** Store prices as integer ticks, never as `double`.
- **Async logging.** The hot thread copies raw binary arguments into a ring buffer, and a
  background thread formats and writes them.
- **Timing.** `rdtsc` (invariant TSC) gives ns-resolution timestamps for a few ns each.
  `clock_gettime` costs ~20 ns.
- **Build.** `-O3 -march=native`, LTO and PGO. Read the assembly of hot code (Compiler Explorer,
  `perf annotate`).
- **Keep it warm.** In a quiet market, hot code and data fall out of cache. Firms run the decision
  path continuously on dummy data, without sending orders, so the real event finds everything hot.
- **Order book layout.** Per symbol, keep an array of price levels indexed by
  `(price - base) / tick` for O(1) updates, plus an open-addressing hash map from order ID to
  order for cancels and executions.

---

## 6. Measuring

Measure before and after every change. Intuition about performance is usually wrong.

- **Histograms, not averages.** Record every sample into an HDR histogram and report
  p50 / p99 / p99.9 / max.
- **Microbenchmarks** for each component (parser, book, queue).
- **End-to-end replay.** Push recorded market data through the full pipeline at real speed and at
  burst speed. The market is bursty, and the worst case is what counts.
- **Tools.** `perf stat` / `perf record`, `perf c2c` (false sharing), Intel VTune, and
  `cyclictest` / `hwlatdetect` for OS and hardware jitter.
- **In production**, use hardware timestamps from taps on the inbound and outbound wire.

---

## 7. Correctness, risk, compliance

Speed is worthless if the system is wrong.

- **Regulation.** In the US, SEC Rule 15c3-5 (Market Access Rule) requires pre-trade risk
  controls, and trades are reported to CAT. In the EU, MiFID II RTS 6 requires algorithm testing,
  kill switches and throttles, and RTS 25 sets the clock-sync rules.
- **Exchange certification.** Every venue runs conformance tests before it lets you connect.
- **Cautionary tale.** In August 2012 Knight Capital lost ~$440M in 45 minutes because a
  deployment reached only some servers and reactivated dead code. That is why kill switches and
  deployment discipline exist.
- **Determinism.** The same code should run live, in backtest and in replay. Single-threaded,
  event-ordered processing per stage makes every bug reproducible.

---

## 8. How to build it: roadmap

This plan is for one developer without colocation. Every phase ends with a number you can measure.

**Environment.** On Windows 11, develop in **WSL2 (Ubuntu)** with GCC or Clang and CMake. WSL2
runs as a VM, so its latency numbers are noisy. For real benchmarking and tuning (phase 6
onward), use bare-metal Linux: dual-boot or a spare machine.

| Phase | Build | Done when |
|-------|-------|-----------|
| 0 | CMake project (C++20, `-O3 -march=native`), `rdtsc` timer, latency histogram | You can time any code in ns and print p50/p99/p99.9 |
| 1 | Lock-free SPSC ring buffer | Core-to-core message latency benchmarked between two pinned threads |
| 2 | ITCH 5.0 parser: download a free sample day from NASDAQ (`emi.nasdaq.com/ITCH/`) and the spec PDF | A full day (hundreds of millions of messages) parses; ns/msg reported |
| 3 | L3 order book for every symbol, built from ITCH | Invariants hold (book never crossed, sizes ≥ 0); per-update latency histogram |
| 4 | Exchange simulator: replays ITCH over UDP multicast on localhost, plus a toy matching engine that accepts OUCH-style binary orders over TCP | The full loop runs end to end |
| 5 | Strategy stub, pre-trade risk and order gateway on pinned threads joined by SPSC queues | Tick-to-trade histogram measured |
| 6 | Linux tuning from section 4, on bare metal | A before/after histogram for each change; p99.9 shrinks |
| 7 | Kernel bypass: AF_XDP or DPDK (common Intel NICs), or Onload/ef_vi on a used Solarflare card | Kernel-bypass vs kernel-socket latency compared |
| 8 | *(Optional)* FPGA feed parser | — |

---

## Reading list

- *Building Low Latency Applications with C++*, Sourav Ghosh: builds a full trading stack in C++, closest to this project
- *Developing High-Frequency Trading Systems*, Donadio, Ghosh, Rossier
- *Trading and Exchanges*, Larry Harris: market microstructure, i.e. why the system looks like this
- Carl Cook, "When a Microsecond Is an Eternity" (CppCon 2017 talk)
- David Gross, "Trading at Light Speed" (talk)
- Ulrich Drepper, *What Every Programmer Should Know About Memory* (free)
- Agner Fog's optimization manuals (free)
- LMAX Disruptor paper, and Martin Thompson's *Mechanical Sympathy* blog
- NASDAQ TotalView-ITCH 5.0 and OUCH specifications (free PDFs)
