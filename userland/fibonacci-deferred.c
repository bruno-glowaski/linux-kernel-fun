#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

typedef atomic_size_t atomic_t;

#include "../fibonacci-deferred.h"
#include "./backoff.h"

#define DEVICE_PATH "/dev/deffib"

int main(void) {
  int r, device;
  void *page;
  size_t page_size;
  struct fib_page *dst;
  struct backoff b;
  size_t i;
  uint64_t expected;

  page_size = sysconf(_SC_PAGESIZE);
  printf("Allocating a %li bytes long page...\n", page_size);
  r = posix_memalign(&page, page_size, page_size);
  if (r < 0) {
    err(r, NULL);
  }
  memset(page, 0, page_size);

  printf("Opening device...\n");
  device = open(DEVICE_PATH, O_RDWR);
  if (device < 0) {
    err(errno, NULL);
  }

  printf("Sending command using destination %p (%li)...\n", page, (long)page);
  r = ioctl(device, DISPATCH_CMD, page);
  if (r < 0) {
    err(errno, NULL);
  }

  printf("Waiting for completion...\n");
  dst = (struct fib_page *)page;
  printf("Addresses: Base -> %p; Count -> %p; Values -> %p;\n", dst,
         &dst->count, &dst->values);
  b = backoff_create();
  while (atomic_load_explicit(&dst->count, memory_order_acquire) <
         FIBONACCI_COUNT(page_size)) {
    backoff_snooze(&b);
  }

  printf("Kernel work completed! Checking results...\n");
  if (dst->values[0] != 1) {
    printf("Unexpected value at index 0: expected 1 got %li!\n",
           dst->values[0]);
  }
  if (dst->values[1] != 1) {
    printf("Unexpected value at index 1: expected 1 got %li!\n",
           dst->values[1]);
  }
  for (i = 2; i < FIBONACCI_COUNT(page_size); i++) {
    expected = dst->values[i - 1] + dst->values[i - 2];
    if (dst->values[i] != expected) {

      printf("Unexpected value at index %li: expected %li got %li!\n", i,
             expected, dst->values[i]);
    }
  }

  printf("Success!");
  // Program will close, so resources will be freed automatically.
}
