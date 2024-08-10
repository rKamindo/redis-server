#ifndef REDIS_RING_BUFFER_H
#define REDIS_RING_BUFFER_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ring_buffer_struct;
typedef struct ring_buffer_struct *ring_buffer;

/**
 * Create a ring_buffer with size. Size must be a multiple of the page size. Return -1 on error.
 */
int rb_create(size_t size, ring_buffer *);

/**
 * Destroy a ring_buffer. Return -1 on error.
 */
int rb_destroy(ring_buffer);

/**
 * Get a pointer to a readable region of memory. Return -1 on error.
 */
int rb_readable(ring_buffer, char **, size_t *);

/**
 * Get a pointer to a writable region of memory. Return -1 on error.
 */
int rb_writable(ring_buffer, char **, size_t *);

/**
 * Advance the read pointer. Return -1 on error.
 */
int rb_read(ring_buffer, size_t);

/**
 * Advance the write pointer. Return -1 on error.
 */
int rb_write(ring_buffer, size_t);

#ifdef __cplusplus
}
#endif

#endif // REDIS_RING_BUFFER_H
