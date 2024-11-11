#include "linux/stddef.h"
#include <linux/types.h>

struct fib_page {
  atomic_t count;
  uint64_t values[];
};

#define DISPATCH_CMD _IOW(0xABCD, 0, struct fib_page *)
#define FIBONACCI_COUNT(page_size)                                             \
  ((page_size - offsetof(struct fib_page, values)) / sizeof(uint64_t))
