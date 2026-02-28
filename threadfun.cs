using System;
using System.Threading;
using System.Threading.Tasks;

internal static class Program
{
    private const int WorkerCount = 4;
    private const int JobCount = 20;

    private readonly struct WorkerContext
    {
        public WorkerContext(int[] jobs, int start, int end, CancellationToken cancelToken)
        {
            Jobs = jobs;
            Start = start;
            End = end;
            CancelToken = cancelToken;
        }

        public int[] Jobs { get; }
        public int Start { get; }
        public int End { get; }
        public CancellationToken CancelToken { get; }
    }

    private static void Worker(WorkerContext ctx)
    {
        for (int i = ctx.Start; i < ctx.End; i++)
        {
            if (ctx.CancelToken.IsCancellationRequested)
            {
                return;
            }
            Console.WriteLine($"Thread {Environment.CurrentManagedThreadId} processing {ctx.Jobs[i]}");
            Thread.Sleep(200);
        }
    }

    private static void Main()
    {
        using var cts = new CancellationTokenSource();
        var jobs = new int[JobCount];
        for (int i = 0; i < JobCount; i++)
        {
            jobs[i] = i;
        }

        var workers = new Task[WorkerCount];

        for (int i = 0; i < WorkerCount; i++)
        {
            int start = (i * JobCount) / WorkerCount;
            int end = ((i + 1) * JobCount) / WorkerCount;

            // Static partition: pass each task its own [start, end) slice context.
            var ctx = new WorkerContext(jobs, start, end, cts.Token);
            workers[i] = Task.Run(() => Worker(ctx));
        }

        // Early-stop path: cts.Cancel();
        // Join all workers (WaitForMultipleObjects(..., TRUE, ...) equivalent).
        Task.WaitAll(workers);
    }
}
