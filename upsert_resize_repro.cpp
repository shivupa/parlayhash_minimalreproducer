// Reproducer: concurrent Upsert on existing keys while the table grows.
//
// Several threads Upsert (increment) keys into a small-initial-size
// parlay_unordered_map so that it has to grow several times. Each thread
// works on a contiguous range of operations; every key is hit many times, so
// most Upserts hit an existing key.
//
// Expected: size() == num_keys and the values sum to num_ops.
// Observed (081408b): segfault in check_bucket_and_state (null
// table_version::next), or lost entries / wrong counts.

#include <parlay_hash/unordered_map.h>

#include <cstdio>
#include <cstdlib>
#include <optional>
#include <thread>
#include <vector>

int main(int argc, char** argv) {
  long init_size = argc > 1 ? atol(argv[1]) : 1000;
  int num_threads = argc > 2 ? atoi(argv[2]) : 8;
  long num_keys = 200000;
  long num_ops = 4000000;

  parlay::parlay_unordered_map<size_t, long> m(init_size);

  std::vector<std::thread> threads;
  long chunk = num_ops / num_threads;
  for (int t = 0; t < num_threads; t++)
    threads.emplace_back([&, t] {
      long end = (t == num_threads - 1) ? num_ops : (t + 1) * chunk;
      for (long i = t * chunk; i < end; i++) {
        size_t k = (i * 2654435761ul) % num_keys;
        m.Upsert(k, [](const std::optional<long>& v) { return v ? *v + 1 : 1; });
      }
    });
  for (auto& th : threads) th.join();

  long size = m.size();
  long total = 0;
  for (long k = 0; k < num_keys; k++) total += m.Find(k).value_or(0);

  bool ok = (size == num_keys) && (total == num_ops);
  printf("init_size=%ld threads=%d size=%ld (expected %ld) sum=%ld (expected %ld) -> %s\n",
         init_size, num_threads, size, num_keys, total, num_ops, ok ? "OK" : "FAIL");
  return ok ? 0 : 1;
}
