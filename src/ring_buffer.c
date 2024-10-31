/*
This ring buffer implementation was made by buckfullingham: https://github.com/buckfullingham
*/
#include "ring_buffer.h"

#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

struct ring_buffer_struct {
  uint64_t read_index;
  uint64_t write_index;
  char *region;
  size_t size;
  int fd;
};

#define ASSERT_STATE(rb)                                                                           \
  assert(rb);                                                                                      \
  assert(rb->fd != -1);                                                                            \
  assert(rb->region);                                                                              \
  assert(rb->read_index <= rb->write_index);                                                       \
  assert(rb->write_index - rb->read_index <= rb->size);

static size_t readable_len(ring_buffer rb) { return rb->write_index - rb->read_index; }

static size_t writable_len(ring_buffer rb) { return rb->size - readable_len(rb); }

int rb_create(size_t size, ring_buffer *result) {
  const long page_size = sysconf(_SC_PAGESIZE);
  void *map1, *map2;
  char *region;
  struct ring_buffer_struct *rb;

  if (size % page_size) return -1;

  rb = (ring_buffer)malloc(sizeof(struct ring_buffer_struct));

  if (!rb) return -1;

  rb->read_index = 0;
  rb->write_index = 0;
  rb->region = NULL;
  rb->size = size;
  rb->fd = -1;

  char name[64];

  if (snprintf(name, sizeof(name), "rb.%d.%p", (int)getpid(), rb) >= sizeof(name)) {
    rb_destroy(rb);
    return -1;
  }

  rb->fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL | O_TRUNC, S_IRUSR | S_IWUSR);

  if (rb->fd == -1) {
    rb_destroy(rb);
    return -1;
  }

  if (shm_unlink(name) == -1) {
    rb_destroy(rb);
    return -1;
  }

  if (ftruncate(rb->fd, (off_t)size) == -1) {
    rb_destroy(rb);
    return -1;
  }

  map1 = mmap(NULL, 2 * size, PROT_READ | PROT_WRITE, MAP_SHARED, rb->fd, 0);

  if (map1 == MAP_FAILED) {
    rb_destroy(rb);
    return -1;
  } else {
    rb->region = (char *)map1;
  };

  map2 = mmap(rb->region + size, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, rb->fd, 0);

  if (map2 == MAP_FAILED) {
    rb_destroy(rb);
    return -1;
  }

  ASSERT_STATE(rb);
  *result = rb;
  return 0;
}

int rb_destroy(ring_buffer rb) {
  if (!rb) return -1;

  if (rb->fd != -1) close(rb->fd);

  if (rb->region != NULL) {
    munmap(rb->region, rb->size * 2);
  }

  free(rb);

  return 0;
}

int rb_readable(ring_buffer rb, char **buf, size_t *len) {
  ASSERT_STATE(rb);
  *buf = rb->region + (rb->read_index % rb->size);
  *len = readable_len(rb);
  return 0;
}

int rb_writable(ring_buffer rb, char **buf, size_t *len) {
  ASSERT_STATE(rb);
  *buf = rb->region + (rb->write_index % rb->size);
  *len = writable_len(rb);
  return 0;
}

int rb_read(ring_buffer rb, size_t len) {
  ASSERT_STATE(rb);
  if (len > readable_len(rb)) return -1;
  rb->read_index += len;
  ASSERT_STATE(rb);
  return 0;
}

int rb_write(ring_buffer rb, size_t len) {
  ASSERT_STATE(rb);
  if (len > writable_len(rb)) return -1;
  rb->write_index += len;
  ASSERT_STATE(rb);
  return 0;
}
