# Win32 to Go/C#: Concurrency Memory Model Cheat Sheet

A reminder to myself when switching between languages :) 

Files in this repo:

**Static partition (disjoint ownership, no mutex):**
- `threadfun.c`: Win32 threads + startup slice context (`lpParameter`)
- `threadfun.go`: goroutines + startup slice context struct + `WaitGroup`
- `threadfun.cs`: tasks + startup slice context struct + `Task.WaitAll`

**Shared queue (dynamic dispatch, mutex required):**
- `threadfun_queue.c`: Win32 threads + shared queue (`CRITICAL_SECTION`)
- `threadfun_queue.go`: goroutines + shared queue (`sync.Mutex`)
- `threadfun_queue.cs`: tasks + shared queue (`lock`)

All six process 20 jobs with 4 workers and join before exit. The two sets differ in how work is assigned — see Pattern Contrast below.

## Core Memory-Model Reminder

Do not ask "which thread sees this write?"  
Ask "what guarantees visibility and ordering for this write?"

- Static partition: disjoint ownership — no synchronization needed for job data; the join barrier covers completion ordering.
- Shared queue: explicit mutex — every access to shared state must be inside the lock, including the read and the increment together.

## Primitive Mapping (Win32 -> Go -> C#)

| Win32 C | Go | C# |
| --- | --- | --- |
| `CreateThread` | `go f()` | `Task.Run(...)` |
| `CRITICAL_SECTION` | `sync.Mutex` | `lock` / `Monitor` |
| startup `lpParameter` context | startup worker context struct arg | startup worker context struct arg |
| fixed `[start,end)` partition | fixed `[start,end)` partition | fixed `[start,end)` partition |
| `WaitForMultipleObjects(..., TRUE, ...)` | `WaitGroup.Wait()` | `Task.WaitAll(...)` |

## Pattern Contrast

| | Static partition (`threadfun.*`) | Shared queue (`threadfun_queue.*`) |
| --- | --- | --- |
| Work assignment | Fixed `[start,end)` slice at startup | Dynamic: each worker claims next available job |
| Mutex needed? | No — disjoint ownership | Yes — shared `nextJob` index |
| Load balance | Fixed (uneven if jobs vary in cost) | Natural (faster workers take more jobs) |
| Complexity | Lower | Higher (requires correct lock discipline) |

Start with static partition. Reach for a shared queue when job cost varies significantly or the job count isn't known at startup.

## Win32 Habit -> Go/C# Replacement

**Static partition:**

- Habit: pass `NULL` at thread startup and rely on globals
- Replacement: pass explicit startup context (`lpParameter`, function args)

- Habit: implicit work ownership
- Replacement: explicit per-worker ownership (`start/end` partition)

- Habit: reason from thread identity
- Replacement: reason from ownership transfer + join barrier

**Shared queue:**

- Habit: unprotected shared counter or index
- Replacement: wrap read-increment-release of shared index in a single critical section / mutex lock

- Habit: check-then-act without holding the lock
- Replacement: claim the job while still holding the lock; release only after the index is updated

## Go-Specific Memory Reminder

**Static partition (`threadfun.go`):**

- pass immutable worker context by value
- avoid shared mutable indexes/queues unless workload requires dynamic balancing
- keep `WaitGroup` as the explicit join barrier

**Shared queue (`threadfun_queue.go`):**

- encapsulate the shared index and its mutex together in a struct; don't expose the mutex directly
- `pop` must hold the lock for the full read-increment sequence, not just the read
- note: a `chan int` (buffered, pre-loaded) is the idiomatic Go alternative to a mutex-guarded index for this pattern

## C#-Specific Memory Reminder

**Static partition (`threadfun.cs`):**

- pass worker context object/struct at task launch
- prefer immutable context fields (`readonly struct`) for clarity
- keep `Task.WaitAll` as the explicit completion barrier

**Shared queue (`threadfun_queue.cs`):**

- encapsulate the shared index and its lock object together in the queue class; don't expose either directly
- `TryPop` must hold the lock for the full read-increment sequence
- `lock` (`Monitor`) is appropriate here; `Interlocked.Increment` is an alternative if the only shared state is the counter itself

## Practical Review Checklist

For each shared value, confirm one of:

- single owner only (no concurrent sharing)
- protected by lock/mutex/monitor
- transferred by startup context handoff

**Static partition — lifecycle:**

- partitioning logic covers all jobs exactly once
- worker function bounds are correct (`start <= i < end`)
- main thread joins workers before process exit

**Shared queue — lifecycle:**

- `TryPop`/`pop` holds the lock across the full read-increment (no gap between check and update)
- queue is fully populated before any worker starts, or writes to the queue are also protected
- main thread joins all workers before process exit

## Why This Comparison Helps

- The static partition files show the same startup context transfer pattern across three languages — compare language/runtime differences without mixing in synchronization design differences.
- The shared queue files show the same mutex-guarded dynamic dispatch pattern across three languages — compare language/runtime differences without mixing in partitioning differences.
- Pairing the two sets makes the design trade-off concrete: disjoint ownership eliminates synchronization; shared state requires it.

Mental shift: reach for a mutex when you genuinely have shared mutable state, not as a default.
