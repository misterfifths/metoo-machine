COMPONENT_ADD_INCLUDEDIRS := liboauth/src
COMPONENT_SRCDIRS := liboauth/src
COMPONENT_SUBMODULES := liboauth
CFLAGS += -DUSE_BUILTIN_HASH

# Disable a warning, and also newlib's RAND_MAX is defined as __RAND_MAX,
# which is undefined from here for some reason
liboauth/src/oauth.o: CFLAGS += -Wno-char-subscripts -D__RAND_MAX=0x7fffffff
