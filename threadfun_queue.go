package main

// Dynamic dispatch via shared queue.
// Contrast with threadfun.go, which uses a static [start,end) partition.
//
// Here all goroutines share a single sharedQueue. The sync.Mutex protects
// the read-increment-release of nextJob so no job is claimed by two goroutines
// and no job is skipped.
//
// Note: a channel-based queue (chan int) is the more idiomatic Go equivalent,
// but sync.Mutex is used here to parallel the C and C# versions directly.

import (
	"context"
	"fmt"
	"sync"
	"time"
)

const (
	workerCount = 4
	jobCount    = 20
)

type sharedQueue struct {
	mu      sync.Mutex
	jobs    []int
	nextJob int
}

func (q *sharedQueue) pop() (int, bool) {
	q.mu.Lock()
	defer q.mu.Unlock()
	if q.nextJob >= len(q.jobs) {
		return 0, false
	}
	job := q.jobs[q.nextJob]
	q.nextJob++
	return job, true
}

func worker(id int, wg *sync.WaitGroup, ctx context.Context, q *sharedQueue) {
	defer wg.Done()
	for {
		select {
		case <-ctx.Done():
			return
		default:
		}
		job, ok := q.pop()
		if !ok {
			return
		}
		fmt.Printf("Worker %d processing %d\n", id, job)
		time.Sleep(200 * time.Millisecond)
	}
}

func main() {
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	jobs := make([]int, jobCount)
	for i := range jobs {
		jobs[i] = i
	}

	// All goroutines receive the same pointer — synchronization is inside the queue.
	q := &sharedQueue{jobs: jobs}

	var wg sync.WaitGroup
	for i := 0; i < workerCount; i++ {
		wg.Add(1)
		go worker(i, &wg, ctx, q)
	}

	// Early-stop path: cancel()
	wg.Wait()
}
