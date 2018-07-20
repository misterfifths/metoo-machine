#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

# newlib's ctype.h triggers this warning
rolling_buffer.o: CFLAGS += -Wno-char-subscripts