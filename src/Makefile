
# --------------------------------------------------------------------------
# VERBOSE Compile Information 
ifdef VERBOSE
        VERBOSE = true
        VERBOSE_ECHO = @ echo
        VERBOSE_SHOW =
        QUIET_ECHO = @ echo > /dev/null
else
        VERBOSE = false
        VERBOSE_ECHO = @ echo > /dev/null
        VERBOSE_SHOW = @
        QUIET_ECHO = @ echo
endif


# --------------------------------------------------------------------------
# BUILD directory
ifndef BUILD
    ifdef DEBUG
        BUILD := build-debug
    else
        BUILD := build
    endif
endif

# --------------------------------------------------------------------------
# Acquire configuration information for libraries that libs3 depends upon

ifndef CURL_LIBS
	CURL_LIBS := $(shell curl-config --libs)
endif

ifndef LIBXML2_LIBS
    LIBXML2_LIBS := $(shell xml2-config --libs)
endif

# --------------------------------------------------------------------------
# Setup FUSE flags 

FUSE_CFLAGS := -D_FILE_OFFSET_BITS=64 \
					 		 -DFUSE_USE_VERSION=26 \
					     -I/usr/local/include/fuse \
					     -I/usr/include/fuse	\
								-Ibin/obj		\
								-Icloud-lib	\
								-Idedup-lib \
								-Isnapshot

FUSE_LIBS := -lfuse

# --------------------------------------------------------------------------
# These CFLAGS assume a GNU compiler.  For other compilers, write a script
# which converts these arguments into their equivalent for that particular
# compiler.

ifndef CFLAGS
    ifdef DEBUG
        CFLAGS := -g
    else
        CFLAGS := -g #-O3
    endif
endif

#          $(CURL_CFLAGS) $(LIBXML2_CFLAGS) 
	
CFLAGS += -Wall  -Wshadow -Wextra -Iinclude \
					$(FUSE_CFLAGS) \
          -D__STRICT_ANSI__ \
          -D_ISOC99_SOURCE \
          -D_POSIX_C_SOURCE=200112L \
					-lstdc++

CPPFLAGS := -Wall  -Wshadow -Wextra -Iinclude \
          $(FUSE_CFLAGS) \
          -D__STRICT_ANSI__ \
          -D_ISOC99_SOURCE \
          -D_POSIX_C_SOURCE=200112L 	\
					-Wno-write-strings		\
					-g
 
CPPLD := -Wall `pkg-config fuse --libs` -lcl

ifndef CC
    ifdef DEBUG
        CC := -g
    else
        CC := -O3
    endif
endif


LDFLAGS = $(CURL_LIBS) $(LIBXML2_LIBS) $(FUSE_LIBS) -lpthread -lcrypto -lssl -lcurl -ltar 
LIBRARY = ./lib/libs3.a -lcurl -lxml2
ifdef DEBUG
	LIBRARY += ./lib/libdedup-dbg.a
else
	LIBRARY += ./lib/libdedup.a
endif

# --------------------------------------------------------------------------
# Default targets are everything

.PHONY: all
all: cloudfs 

# --------------------------------------------------------------------------
# Compile target patterns

$(BUILD)/obj/lock.o:cloudfs/lock.cpp
	$(QUIET_ECHO) $@: Compiling object
	@ mkdir -p $(dir $(BUILD)/dep/$<)
	@g++ $(CPPFLAGS) -M -MG -MQ $@ -DCOMPILINGDEPENDENCIES \
        -o $(BUILD)/dep/$(<:%.c=%.d) -c $<
	mkdir -p $(dir $@)
	$(VERBOSE_SHOW) g++ $(CPPFLAGS) -o $@ -c $<

$(BUILD)/obj/cache_controller.o:cloudfs/cache_controller.cpp
	$(QUIET_ECHO) $@: Compiling object
	@ mkdir -p $(dir $(BUILD)/dep/$<)
	@g++ $(CPPFLAGS) -M -MG -MQ $@ -DCOMPILINGDEPENDENCIES \
        -o $(BUILD)/dep/$(<:%.c=%.d) -c $<
	mkdir -p $(dir $@)
	$(VERBOSE_SHOW) g++ $(CPPFLAGS) -o $@ -c $<

$(BUILD)/obj/log.o:cloudfs/log.cpp
	$(QUIET_ECHO) $@: Compiling object
	@ mkdir -p $(dir $(BUILD)/dep/$<)
	@g++ $(CPPFLAGS) -M -MG -MQ $@ -DCOMPILINGDEPENDENCIES \
        -o $(BUILD)/dep/$(<:%.c=%.d) -c $<
	mkdir -p $(dir $@)
	$(VERBOSE_SHOW) g++ $(CPPFLAGS) -o $@ -c $<

$(BUILD)/obj/transmission.o:cloudfs/transmission.cpp
	$(QUIET_ECHO) $@: Compiling object
	@ mkdir -p $(dir $(BUILD)/dep/$<)
	@g++ $(CPPFLAGS) -M -MG -MQ $@ -DCOMPILINGDEPENDENCIES \
        -o $(BUILD)/dep/$(<:%.c=%.d) -c $<
	mkdir -p $(dir $@)
	$(VERBOSE_SHOW) g++ $(CPPFLAGS) -o $@ -c $<

$(BUILD)/obj/snapshot.o:cloudfs/snapshot.cpp
	$(QUIET_ECHO) $@: Compiling object
	@ mkdir -p $(dir $(BUILD)/dep/$<)
	@g++ $(CPPFLAGS) -M -MG -MQ $@ -DCOMPILINGDEPENDENCIES \
        -o $(BUILD)/dep/$(<:%.c=%.d) -c $<
	mkdir -p $(dir $@)
	$(VERBOSE_SHOW) g++ $(CPPFLAGS) -o $@ -c $<

$(BUILD)/obj/duplication.o:cloudfs/duplication.cpp
	$(QUIET_ECHO) $@: Compiling object
	@ mkdir -p $(dir $(BUILD)/dep/$<)
	@g++ $(CPPFLAGS) -M -MG -MQ $@ -DCOMPILINGDEPENDENCIES \
        -o $(BUILD)/dep/$(<:%.c=%.d) -c $<
	mkdir -p $(dir $@)
	$(VERBOSE_SHOW) g++ $(CPPFLAGS) -o $@ -c $<

$(BUILD)/obj/Lancerfs.o:cloudfs/Lancerfs.cpp 
	$(QUIET_ECHO) $@: Compiling object
	@ mkdir -p $(dir $(BUILD)/dep/$<)
	@g++ $(CPPFLAGS) -M -MG -MQ $@ -DCOMPILINGDEPENDENCIES \
        -o $(BUILD)/dep/$(<:%.c=%.d) -c $<
	mkdir -p $(dir $@)
	$(VERBOSE_SHOW) g++ $(CPPFLAGS) -o $@ -c $<

$(BUILD)/obj/wrapper.o: cloudfs/wrapper.cpp 
	$(QUIET_ECHO) $@: Compiling object
	@ mkdir -p $(dir $(BUILD)/dep/$<)
	@g++ $(CPPFLAGS) -M -MG -MQ $@ -DCOMPILINGDEPENDENCIES \
        -o $(BUILD)/dep/$(<:%.c=%.d) -c $<
	mkdir -p $(dir $@)
	$(VERBOSE_SHOW) g++ $(CPPFLAGS) -o $@ -c $<

$(BUILD)/obj/cloudfs.o: cloudfs/cloudfs.c cloudfs/cloudfs.h
	$(QUIET_ECHO) $@: Compiling object
	@ mkdir -p $(dir $(BUILD)/dep/$<)
	@gcc $(CFLAGS) -M -MG -MQ $@ -DCOMPILINGDEPENDENCIES \
        -o $(BUILD)/dep/$(<:%.c=%.d) -c $<
	mkdir -p $(dir $@)
	$(VERBOSE_SHOW) gcc $(CFLAGS) -o $@ -c $<

$(BUILD)/obj/cloudapi.o: cloud-lib/cloudapi.c 
	$(QUIET_ECHO) $@: Compiling object
	@ mkdir -p $(dir $(BUILD)/dep/$<)
	@gcc $(CFLAGS) -M -MG -MQ $@ -DCOMPILINGDEPENDENCIES \
        -o $(BUILD)/dep/$(<:%.c=%.d) -c $<
	mkdir -p $(dir $@)
	$(VERBOSE_SHOW) gcc $(CFLAGS) -o $@ -c $<

$(BUILD)/obj/main.o: cloudfs/main.c cloudfs/cloudfs.h	\
										cloud-lib/cloudapi.c 
	$(QUIET_ECHO) $@: Compiling object
	@ mkdir -p $(dir $(BUILD)/dep/$<)
	@gcc $(CFLAGS) -M -MG -MQ $@ -DCOMPILINGDEPENDENCIES \
        -o $(BUILD)/dep/$(<:%.c=%.d) -c $<
	mkdir -p $(dir $@)
	$(VERBOSE_SHOW) gcc $(CFLAGS) -o $@ -c $<

$(BUILD)/obj/%.o: dedup-lib/%.c
	$(QUIET_ECHO) $@: Compiling object
	@ mkdir -p $(dir $(BUILD)/dep/$<)	
	@ gcc $(CFLAGS) -M -MG -MQ $@ -DCOMPILINGDEPENDENCIES \
        -o $(BUILD)/dep/$(<:%.c=%.d) -c $<
	@ mkdir -p $(dir $@)
	$(VERBOSE_SHOW) gcc $(CFLAGS) -o $@ -c $<

# --------------------------------------------------------------------------
# CloudFS targets

.PHONY: cloudfs 
cloudfs: $(BUILD)/bin/cloudfs

CLOUDFS_OBJS = $(BUILD)/obj/cloudapi.o \
				$(BUILD)/obj/log.o	\
				$(BUILD)/obj/lock.o	\
				$(BUILD)/obj/cache_controller.o	\
				$(BUILD)/obj/transmission.o	\
				$(BUILD)/obj/snapshot.o	\
				$(BUILD)/obj/duplication.o	\
				$(BUILD)/obj/Lancerfs.o \
        $(BUILD)/obj/wrapper.o \
				$(BUILD)/obj/cloudfs.o \
			   $(BUILD)/obj/main.o 
					
#You can append other objects

$(BUILD)/bin/cloudfs: $(CLOUDFS_OBJS)
	$(QUIET_ECHO) $@: Building executable
	@ mkdir -p $(dir $@)
	$(VERBOSE_SHOW) g++ -o $(BUILD)/bin/cloudfs $^ $(LDFLAGS) $(LIBRARY) 

# --------------------------------------------------------------------------
# Example targets

.PHONY: cloud-example 
cloud-example: $(BUILD)/bin/cloud-example

CLOUDFS_OBJS = $(BUILD)/obj/cloudapi.o \
							 $(BUILD)/obj/cloud-example.o

$(BUILD)/bin/cloud-example: $(CLOUDFS_OBJS)
	$(QUIET_ECHO) $@: Building executable
	@ mkdir -p $(dir $@)
	$(VERBOSE_SHOW) gcc -o $@ $^ $(LDFLAGS) $(LIBRARY)

.PHONY: rabin-example 
rabin-example: $(BUILD)/bin/rabin-example

CLOUDFS_OBJS = $(BUILD)/obj/rabin-example.o

$(BUILD)/bin/rabin-example: $(CLOUDFS_OBJS)
	$(QUIET_ECHO) $@: Building executable
	@ mkdir -p $(dir $@)
	$(VERBOSE_SHOW) gcc -o $@ $^ $(LDFLAGS) $(LIBRARY) 

# --------------------------------------------------------------------------
# Clean target

.PHONY: clean
clean:
	$(QUIET_ECHO) $(BUILD): Cleaning
	$(VERBOSE_SHOW) rm -rf $(BUILD)
