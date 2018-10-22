// 2018 / Tim Clem / github.com/misterfifths
// Public domain.

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>  // for isprint()

#include "rolling_buffer.h"


struct rbuf {
	size_t length;  // length of bytes
	size_t next_write_index;  // index into bytes of next write (and thus also the count of valid bytes)
	char *bytes;
};


rbuf *rbuf_alloc(size_t length)
{
	assert(length != 0);

	rbuf *buffer = malloc(sizeof(rbuf));

	buffer->length = length;
	buffer->next_write_index = 0;

	// Allocate an extra byte for a safety nul
	buffer->bytes = malloc(sizeof(char) * (length + 1));
	buffer->bytes[0] = '\0';
	buffer->bytes[length] = '\0';

	return buffer;
}

void rbuf_free(rbuf *buffer)
{
	assert(buffer != NULL);

	free(buffer->bytes);
	free(buffer);
}

void rbuf_reset(rbuf *buffer)
{
	assert(buffer != NULL);

	buffer->next_write_index = 0;
	buffer->bytes[0] = '\0';
}

size_t rbuf_get_length(const rbuf *buffer)
{
	return buffer->length;
}

size_t rbuf_get_valid_byte_count(const rbuf *buffer)
{
	return buffer->next_write_index;
}

char *rbuf_get_bytes(const rbuf *buffer)
{
	return buffer->bytes;
}

void rbuf_discard_bytes(rbuf *buffer, size_t byte_count)
{
	// Shuffle everything after byte_count up to the front.
	// Add a nul byte after it, for safety's sake.
	// Adjust the values in the struct.

	assert(buffer != NULL);
	assert(byte_count <= buffer->next_write_index);

	if(byte_count == 0) return;


	// 0 1 2 3 4 5
	// a b c d e f

	// discard 2 ==> move 6 - 2 = 4 bytes starting at index 2 to index 0
	// 0 1 2 3 4 5
	// c d e f e f

	// add nul at index 4

	size_t remaining_byte_count = buffer->next_write_index - byte_count;
	if(remaining_byte_count > 0) {
		// memmove, not memcpy; the ranges will overlap if it's more than half the buffer
		memmove(buffer->bytes, buffer->bytes + byte_count, remaining_byte_count);
	}

	buffer->bytes[remaining_byte_count] = '\0';
	buffer->next_write_index = remaining_byte_count;
}

void rbuf_discard_bytes_ending_at(rbuf *buffer, const char *last_byte_to_discard)
{
	assert(last_byte_to_discard >= buffer->bytes);

	size_t offset = 1 + last_byte_to_discard - buffer->bytes;
	rbuf_discard_bytes(buffer, offset);
}

void rbuf_get_write_info(const rbuf *buffer, char **next_write_start, size_t *max_write_length)
{
	assert(next_write_start != NULL);
	assert(max_write_length != NULL);

	*next_write_start = buffer->bytes + buffer->next_write_index;
	*max_write_length = buffer->length - buffer->next_write_index;
}

void rbuf_add_bytes(rbuf *buffer, size_t bytes_written)
{
	assert(buffer->next_write_index + bytes_written <= buffer->length);

	buffer->next_write_index += bytes_written;
	buffer->bytes[buffer->next_write_index] = '\0';  // Even if we just filled the whole buffer, there's a spare byte at the end, so this won't overflow
}

void rbuf_dump(const rbuf *buffer)
{
	printf("Rolling buffer, length %zu @ %p. Next write at offset %zu.", buffer->length, buffer->bytes, buffer->next_write_index);
	for(size_t i = 0; i < buffer->length; i++) {
		if(i == buffer->next_write_index) printf("\n** End of valid data **\n");
		else if(i % 16 == 0) printf("\n");

		char byte = buffer->bytes[i];
		if(isprint(byte)) printf("%2c ", byte & 0xff);
		else printf("%02x ", byte);
	}

	puts("\n");
}
