// 2018 / Tim Clem / github.com/misterfifths
// Public domain.

#ifndef _ROLLING_BUFFER_H
#define _ROLLING_BUFFER_H


#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>


// A buffer of fixed size that is filled by appending bytes, and intermittently
// has bytes removed from the beginning (causing the remaining bytes to shift left).
// Slightly specialized for holding strings; the bytes are guaranteed to be null-terminated.

// Bytes are added to the buffer by calling rbuf_get_write_info to retrieve a location
// and length for a write, and then moving bytes to that location via some external means
// (memcpy or whatever). Once that's complete, call rbuf_add_bytes to update the structure
// with the number of bytes you added.

typedef struct rbuf rbuf;


// Creates and returns new buffer with the given max length.
rbuf *rbuf_alloc(size_t length);

// Frees a buffer allocated with rbuf_alloc.
void rbuf_free(rbuf *buffer);

// Resets a buffer for reuse.
// Zeroes the valid byte count of the buffer, and sets the first byte to zero.
void rbuf_reset(rbuf *buffer);


// Returns the maximum length of the buffer.
size_t rbuf_get_length(const rbuf *buffer);

// Returns the number of valid bytes currently stored in the buffer (i.e., those
// added via calls to rbuf_add_bytes).
size_t rbuf_get_valid_byte_count(const rbuf *buffer);

// Returns a pointer to the bytes stored in the buffer.
// This is guaranteed to be null-terminated at the end of the valid bytes.
char *rbuf_get_bytes(const rbuf *buffer);


// Removes ("pops") the given number of bytes from the beginning of the buffer by
// shifting the following bytes to the left and decrementing the valid byte count.
void rbuf_discard_bytes(rbuf *buffer, size_t byte_count);

// Removes all bytes up to *and including* the given pointer.
void rbuf_discard_bytes_ending_at(rbuf *buffer, const char *last_byte_to_discard);


// Get information for writing bytes to the buffer.
// After calling this, use memcpy or some other means to copy bytes to next_write_start,
// and then call rbuf_add_bytes to update the structure.
void rbuf_get_write_info(const rbuf *buffer, char **next_write_start, size_t *max_write_length);

// Update the structure by updating the valid byte count to include the given number of bytes.
// Call this after writing bytes to the buffer. This call will ensure the valid bytes remain
// null-terminated.
void rbuf_add_bytes(rbuf *buffer, size_t bytes_written);


// Prints a detailed description of the buffer and its contents to stdout.
void rbuf_dump(const rbuf *buffer);


#endif
