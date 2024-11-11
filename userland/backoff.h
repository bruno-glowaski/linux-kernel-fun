// #define _GNU_SOURCE
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
// #include <sys/types.h>
// #include <unistd.h>

const uint32_t SPIN_LIMIT = 6;
const uint32_t YIELD_LIMIT = 10;

struct backoff {
  uint32_t step;
};

static inline struct backoff backoff_create() {
  const struct backoff b = {.step = 0};
  return b;
}

static inline void busy_loop(uint32_t s) {

  for (uint32_t i = 0; i < s; i++) {
    __asm__ __volatile__("" : "+g"(i) : :);
  }
}

static inline void backoff_spin(struct backoff *self) {
  // printf("%x: spinning...\n", gettid());
  uint32_t s = self->step < SPIN_LIMIT ? self->step : SPIN_LIMIT;
  busy_loop(1 << s);
  if (self->step <= SPIN_LIMIT) {
    self->step++;
  }
}

static inline void backoff_snooze(struct backoff *self) {
  // printf("%x: snoozing...\n", gettid());
  if (self->step <= SPIN_LIMIT) {
    busy_loop(1 << self->step);
  } else {
    sched_yield();
  }

  if (self->step <= YIELD_LIMIT) {
    self->step++;
  }
}
