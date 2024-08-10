#include "ring_buffer.h"
#include <gtest/gtest.h>

TEST(ring_buffer_test, create_and_destroy)
{
  ring_buffer rb;
  // create and destroy
  EXPECT_EQ(rb_create(1<<12, &rb), 0);
  EXPECT_EQ(rb_destroy(rb), 0);
  // do it a second time and it works
  EXPECT_EQ(rb_create(1<<12, &rb), 0);
  EXPECT_EQ(rb_destroy(rb), 0);
}

TEST(ring_buffer_test, initial_state)
{
  ring_buffer rb;
  char *read_ptr;
  char *write_ptr;
  std::size_t read_len;
  std::size_t write_len;

  EXPECT_EQ(rb_create(1<<12, &rb), 0);

  EXPECT_EQ(rb_readable(rb, &read_ptr, &read_len), 0);
  EXPECT_EQ(read_len, 0);

  EXPECT_EQ(rb_writable(rb, &write_ptr, &write_len), 0);
  EXPECT_EQ(write_len, 1<<12);

  EXPECT_EQ(rb_destroy(rb), 0);
}

TEST(ring_buffer_test, write_and_read)
{
  ring_buffer rb;
  char *read_ptr;
  char *write_ptr;
  std::size_t read_len;
  std::size_t write_len;
  int n;

  EXPECT_EQ(rb_create(1<<12, &rb), 0);

  EXPECT_EQ(rb_writable(rb, &write_ptr, &write_len), 0);
  EXPECT_EQ(write_len, 1<<12);

  n = snprintf(write_ptr, write_len, "hello world");

  EXPECT_STREQ(write_ptr, write_ptr + (1<<12));
  EXPECT_EQ(rb_write(rb, n + 1), 0);
  EXPECT_EQ(rb_readable(rb, &read_ptr, &read_len), 0);
  EXPECT_EQ(read_len, n + 1);
  EXPECT_STREQ(read_ptr, write_ptr);
  EXPECT_EQ(rb_read(rb, n + 1), 0);

  rb_readable(rb, &read_ptr, &read_len);
  rb_writable(rb, &write_ptr, &write_len);

  EXPECT_EQ(read_len, 0);
  EXPECT_EQ(write_len, 1<<12);

  EXPECT_EQ(rb_destroy(rb), 0);
}
