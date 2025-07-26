#include "rdb.h"
#include "client.h"
#include "commands.h"
#include "database.h"
#include "redis-server.h"
#include "server_config.h"
#include "util.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>

char *read_rdb_string(FILE *file);
int write_rdb_string(FILE *file, const char *str);

int rdb_load_data_from_file(redis_db_t *db, const char *dir, const char *filename) {
  const char *path = construct_file_path(dir, filename);
  FILE *file = fopen(path, "rb");
  if (!file) {
    perror("failed to open RDB file or it does not exist");
    return -1;
  }

  // consume header section, magic string + version number (ASCII): REDIS0012
  char buf[64];
  if (fread(buf, 1, 9, file) != 9) {
    perror("failed to read header\n");
    fclose(file);
    return 1;
  }
  buf[9] = '\0';

  if (strcmp(buf, "REDIS0012") != 0) {
    printf("header does not contain expected magic string + version number (0012)\n");
    printf("expected: REDIS0012, got: %s\n", buf);
    fclose(file);
    return 1;
  }

  // main RDB section loop
  int done = 0;
  while (!done) {
    int opcode = fgetc(file);
    if (opcode == EOF) {
      perror("unexpected end of file while reading RDB\n");
      break;
    }
    switch (opcode) {
    case 0xFA: // mwetadata section start
      while (1) {
        int next = fgetc(file);
        if (next == 0xFE) {
          // end of metadata section
          break;
        }
        if (next == EOF) {
          perror("unexpected end of file while reading metadata section\n");
          fclose(file);
          return 1;
        }
        ungetc(next, file); // put back for string read
        char *key = read_rdb_string(file);
        if (!key) break;
        char *value = read_rdb_string(file);
        if (!value) {
          free(key);
          break;
        }
        free(key);
        free(value);
      }
      break;
    case 0xFB: { // hash table size info
      int kv_size;
      int exp_size;
      int i;
      int type;
      uint64_t expire_time;

      kv_size = fgetc(file);  // number of key-value pairs in the hash table
      exp_size = fgetc(file); // number of keys with expiry
      if (kv_size == EOF || exp_size == EOF) {
        perror("unexpected end of file while reading hash table size info\n");
        fclose(file);
        return 1;
      }
      // read the key-value pairs
      for (i = 0; i < kv_size; i++) {
        type = fgetc(file); // read type/encoding opcode
        expire_time = 0;

        if (type == EOF) {
          perror("unexpected end of file while reading key-value pairs\n");
          fclose(file);
          return 1;
        }

        // check for expiration
        if (type == 0xFD) { // expire time in seconds
          uint32_t seconds;
          if (fread(&seconds, 4, 1, file) != 1) {
            perror("failed to read expiration time from RDB file\n");
            fclose(file);
            return 1;
          }
          expire_time = seconds * 1000; // convert seconds to milliseconds
          type = fgetc(file);           // read the next type byte
        } else if (type == 0xFC) {      // expire time in milliseconds
          uint64_t milliseconds;
          if (fread(&milliseconds, 8, 1, file) != 1) {
            perror("failed to read expiration time from RDB file\n");
            fclose(file);
            return 1;
          }
          expire_time = milliseconds; // already in milliseconds
          type = fgetc(file);         // read the next type byte
        }

        if (type == 0x00) { // string
          // read the key-value pair as a string
          char *key = read_rdb_string(file);
          if (!key) {
            perror("failed to read key from RDB file\n");
            fclose(file);
            return 1;
          }
          char *value = read_rdb_string(file);
          if (!value) {
            free(key);
            perror("failed to read value from RDB file\n");
            fclose(file);
            return 1;
          }

          // Debug print to verify key and value
          // printf("RDB LOAD: key='%s', value='%s', expire_time=%llu\n", key, value, (unsigned long
          // long)expire_time);

          // insert in DB
          redis_db_set(db, key, value, TYPE_STRING, expire_time);

          free(key);
          free(value);
        } else {
          printf("Unhandled type: 0x%02X\n", type);
        }
      }
      break;
    }
    case 0xFE:
      // database index
      printf("database selector (0xFE) encountered.\n");
      // No support for multiple databases yet, so we just read the index.
      int db_index = fgetc(file);
      if (db_index == EOF) {
        perror("unexpected end of file while reading database index\n");
        fclose(file);
        return 1;
      }
    case 0xFF: // end of RDB file
      done = 1;
      break;
    default:
      break;
    }
  }

  fclose(file);
  return 0;
}

char *read_rdb_string(FILE *file) {
  int first = fgetc(file);
  if (first == EOF) return NULL;
  unsigned char first_byte = (unsigned char)first;

  int type = first_byte >> 6;
  int len = 0;

  if (type == 0b00) {
    // remaining 6 bits is length
    len = first_byte & 0x3F;
  } else if (type == 0b01) {
    // next 14 bits is length
    len = (first_byte & 0x3F) << 8;
    int second = fgetc(file);
    if (second == EOF) return NULL;
    len |= second; // combine with the next byte
  } else if (type == 0b10) {
    // next 4 bytes is length in big-endian
    len = 0;
    for (int i = 0; i < 4; i++) {
      char byte = fgetc(file);
      if (byte == EOF) return NULL;
      len = (len << 8) | (unsigned char)byte; // shift left and add the next byte
    }
  } else if (type == 0b11) {
    // this is a string encoded value
    int enc_type = first_byte & 0x3F;
    char buf[32];
    if (enc_type == 0) {
      // 8-bit integer as string
      int val = fgetc(file);
      if (val == EOF) return NULL;
      snprintf(buf, sizeof(buf), "%d", (int8_t)val);
      return strdup(buf);
    } else if (enc_type == 1) {
      // 16-bit integer as string (little-endian)
      int lo = fgetc(file);
      int hi = fgetc(file);
      if (lo == EOF || hi == EOF) return NULL;
      int16_t val = (hi << 8) | lo;
      snprintf(buf, sizeof(buf), "%d", val);
      return strdup(buf);
    } else if (enc_type == 2) {
      // 32-bit integer as string (little-endian)
      uint32_t val = 0;
      for (int i = 0; i < 4; ++i) {
        int b = fgetc(file);
        if (b == EOF) return NULL;
        val |= ((uint32_t)b) << (8 * i);
      }
      snprintf(buf, sizeof(buf), "%d", (int32_t)val);
      return strdup(buf);
    } else if (enc_type == 3) {
      // LZF-compressed string (not implemented)
      printf("LZF-compressed string not supported.\n");
      return NULL;
    } else {
      return NULL;
    }
  }
  char *str = malloc(len + 1);
  if (!str) {
    perror("failed to allocate memory for string\n");
    return NULL;
  }

  if (fread(str, 1, len, file) != len) {
    perror("failed to read string from file\n");
    free(str);
    return NULL;
  }

  str[len] = '\0';
  return str;
}

int write_rdb_string(FILE *file, const char *str) {
  int len = strlen(str);
  char *endptr;
  long long value = strtoll(str, &endptr, 10);
  if (*endptr == '\0') { // string is a valid integer
    if (value >= INT8_MIN && value <= INT8_MAX) {
      // 8-bit integer encoding: 1 byte header + 1 byte value = 2 bytes (16 bits)
      uint8_t header = 0xC0;
      fputc(header, file);
      fputc((int8_t)value, file);
      return 2;
    } else if (value >= INT16_MIN && value <= INT16_MAX) {
      // 16-bit integer encoding: 1 byte header + 2 bytes value = 3 bytes (24 bits)
      uint8_t header = 0xC1;
      fputc(header, file);
      int16_t v = (int16_t)value;
      fputc(v & 0xFF, file);
      fputc((v >> 8) & 0xFF, file);
      return 3;
    } else if (value >= INT32_MIN && value <= INT32_MAX) {
      // 32-bit integer encoding: 1 byte header + 4 bytes value = 5 bytes (40 bits)
      uint8_t header = 0xC2;
      fputc(header, file);
      int32_t v = (int32_t)value;
      for (int i = 0; i < 4; i++) {
        fputc((v >> (8 * i)) & 0xFF, file);
      }
      return 5;
    }
  }

  if (len <= 63) {
    // 1-byte header + len bytes data = len+1 bytes = (len+1)*8 bits
    uint8_t header = (0b00 << 6) | len;
    fputc(header, file);
    fwrite(str, 1, len, file);
    return 1 + len;
  } else if (len <= 16383) {
    // 2-byte header + len bytes data = len+2 bytes = (len+2)*8 bits
    uint16_t header = (0b01 << 14) | len;
    fputc((header >> 8) & 0xFF, file);
    fputc(header & 0xFF, file);
    fwrite(str, 1, len, file);
    return 2 + len;
  } else {
    // 1-byte header + 4-byte length + len bytes data = len+5 bytes = (len+5)*8 bits
    uint8_t header = (0b10 << 6);
    fputc(header, file);
    uint32_t ulen = (uint32_t)len;
    for (int i = 3; i >= 0; i--) {
      fputc((ulen >> (8 * i)) & 0xFF, file);
    }
    fwrite(str, 1, len, file);
    return 5 + len;
  }
}

bool rdb_save_data_to_file(redis_db_t *db, const char *dir, const char *filename) {
  const char *path = construct_file_path(dir, filename);

  // create directory
  if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
    perror("failed to create directory for RDB file");
    return false;
  }
  FILE *file = fopen(path, "wb");
  if (!file) {
    perror("could not open rdb file");
  }
  // write header:
  fwrite("REDIS0012", 1, 9, file);
  // write metadata section
  fputc(0xFA, file); // start metadata section
  // for each metadata key/value pair:
  // write_rdb_String(file, "meta_key");
  // write_rdb_string(file, "meta_value");
  fputc(0xFE, file); // end metadata section

  // write 0xFB, kv_size, exp-size
  fputc(0xFB, file); // hash table size information
  fputc(db->key_count, file);
  fputc(db->expiry_count, file); // for now I can count all the keys with an expiry

  // for each key:
  // -if has expiry, write 0xFD/0xFC and expiry time
  //  write type
  //  write_rdb_string(key)
  //  write_rdb_string(value)
  khash_t(redis_hash) *h = db->h;
  for (khiter_t k = kh_begin(h); k != kh_end(h); k++) {
    if (!kh_exist(h, k)) continue;
    const char *key = kh_key(h, k);
    RedisValue *val = kh_value(h, k);
    // write expiry if it has
    if (val->expiration > 0) {
      fputc(0xFC, file); // type for ms expiry, since we store all expiry in ms
      uint64_t ms = (uint64_t)val->expiration;
      // Write 8 bytes in little-endian order
      for (int i = 0; i < 8; i++) {
        fputc((ms >> (8 * i)) & 0xFF, file);
      }
    }
    // persist string values (write_rdb_string will handle integer encoding if possible)
    if (val->type == TYPE_STRING) {
      fputc(0x00, file); // type for string
      write_rdb_string(file, key);
      write_rdb_string(file, val->data.str);
    } else {
      // TODO persist other value types
    }
  }

  // write 0xFF
  fputc(0xFF, file);
  // close file
  fclose(file);
  return true;
}

/*
Sends the rdb snapshot to a replica, using the sendfile() syscall, which avoids copying into
userspace.
*/
void master_send_rdb_snapshot(Client *client) {
  if (!client->type == CLIENT_TYPE_REPLICA) {
    perror("cannot send rdb file to a non-replica client\n");
    return;
  }

  size_t remaining_bytes = client->rdb_file_size - client->rdb_file_offset;
  printf("remaining bytes is %ld\n", remaining_bytes);

  while (1) {
    ssize_t bytes_sent =
        sendfile(client->fd, client->rdb_fd, &client->rdb_file_offset, remaining_bytes);
    //  printf("bytes sent: %ld\n", bytes_sent);
    if (bytes_sent == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      } else if (errno == EINTR) {
        continue;
      } else {
        perror("sendfile failed\n");
        return;
      }
    } else if (bytes_sent == 0) {
      break;
    }
  }

  if (client->rdb_file_offset == client->rdb_file_size) {
    printf("RDB file transmission complete for client %d\n", client->fd);
    client_disable_write_events(client);
    client->master_repl_state = MASTER_REPL_STATE_PROPOGATE;
  }
}

void replica_receive_rdb_snapshot(Client *client) {
  ssize_t bytes_read;

  // open a temporary file for writing
  if (client->tmp_rdb_fp == NULL) {
    char *file_path = construct_file_path(g_server_config.dir, "temp_snapshot.rdb");
    client->tmp_rdb_fp = fopen(file_path, "wb");
    if (!client->tmp_rdb_fp) {
      perror("could not open temporary RDB file for writing received snapshot\n");
      return;
    }
    printf("opened temporary file: %s\n", file_path);
    free(file_path);
  }

  // continusly read from socket into ring buffer
  // and drain the ring buffer into the file. This loop will block
  // when no data is available from the socket, or if the ring buffer is full
  while (1) {
    char *write_buf;
    char *read_buf;
    size_t writable_len;
    size_t readable_len;

    // if we haven't finished receiving all the messages
    if (client->rdb_received_bytes < client->rdb_expected_bytes) {
      // get the writable portion of the ring buffer
      if (rb_writable(client->input_buffer, &write_buf, &writable_len) != 0) {
        fprintf(stderr, "failed to get writable buffer\n");
        return;
      }

      if (writable_len == 0) {
        fprintf(stderr, "input buffer full\n");
        return;
      }

      ssize_t remaining_bytes_to_read = client->rdb_expected_bytes - client->rdb_received_bytes;

      bytes_read = read(client->fd, write_buf, remaining_bytes_to_read);

      if (bytes_read == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          return;
        } else if (errno == EINTR) {
          continue;
        } else {
          perror("sendfile failed\n");
          return;
        }
      } else if (bytes_read == 0) {
        break;
      }

      if (rb_write(client->input_buffer, bytes_read)) {
        fprintf(stderr, "failed to update write index\n");
      }

      client->rdb_received_bytes += bytes_read;

      printf("receiving rdb data from master: read %ld bytes into input buffer\n", bytes_read);

      if (bytes_read == 0) {
        // master closed connection. signals end of RDB transfer
        printf("master closed connection\n");
        break;
      } else if (bytes_read == -1) {
        if (errno == EINTR)
          continue; // interrupted retry
        else {
          perror("error reading from master socket.");
          return;
        }
      }
    }

    if (rb_readable(client->input_buffer, &read_buf, &readable_len) != 0) {
      fprintf(stderr, "failed to get readable buffer\n");
      return;
    }

    if (readable_len == 0) {
      fprintf(stderr, "input buffer empty, nothing to write to disk\n");
    }

    // write to disk
    ssize_t bytes_written = fwrite(read_buf, 1, bytes_read, client->tmp_rdb_fp);

    printf("writing rdb data from buffer: wrote %ld bytes into temp file\n", bytes_read);

    client->rdb_written_bytes += bytes_written;
    if (rb_read(client->input_buffer, bytes_written) != 0) {
      fprintf(stderr, "error updating read index for ring buffer\n");
    }

    if (client->rdb_written_bytes == client->rdb_expected_bytes) {
      printf("RDB snapshot completely received and written to temp file: %lld bytes.\n",
             client->rdb_expected_bytes);

      fclose(client->tmp_rdb_fp);

      rdb_load_data_from_file(client->db, g_server_config.dir, "temp_snapshot.rdb");

      client->repl_client_state = REPL_STATE_READY;
      client_enable_read_events(client); // start monitoring for EPOLLIN from master
      return;
    }
  }
}