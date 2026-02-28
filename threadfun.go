package main

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

type workerContext struct {
	id    int
	jobs  []int
	start int
	end   int
}

// ctx is Go's standard cancellation/timeout carrier, kept as a separate first parameter
// by convention so it's immediately visible and compatible with any stdlib/third-party call.
// wctx holds the domain-specific data: which jobs to process and this goroutine's slice of work.
func worker(wg *sync.WaitGroup, ctx context.Context, wctx workerContext) {
	defer wg.Done()

	for i := wctx.start; i < wctx.end; i++ {
		select {
		case <-ctx.Done():
			return
		default:
		}
		fmt.Printf("Worker %d processing %d\n", wctx.id, wctx.jobs[i])
		time.Sleep(200 * time.Millisecond)
	}
}

func main() {
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	jobs := make([]int, jobCount)
	for i := 0; i < jobCount; i++ {
		jobs[i] = i
	}

	var wg sync.WaitGroup

	for i := 0; i < workerCount; i++ {
		start := (i * jobCount) / workerCount
		end := ((i + 1) * jobCount) / workerCount

		// Static partition: pass each goroutine its own [start, end) slice context.
		wctx := workerContext{id: i, jobs: jobs, start: start, end: end}

		wg.Add(1)
		go worker(&wg, ctx, wctx)
	}

	// Early-stop path: cancel()
	wg.Wait()
}
