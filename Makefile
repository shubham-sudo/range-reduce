# Include the RocksDB configuration if required
include lib/rocksdb/make_config.mk

# Define compiler and flags
CXX = g++
CXXFLAGS = -g -O2 -std=c++17 -Ilib/rocksdb/include -Ilib/rocksdb -Iinclude -DTIMER -DPROFILE

# Define libraries and linker flags
LDFLAGS = -lpthread -ldl -lz  # Added -ldl and -lz for dynamic linking and zlib support
EXEC_LDFLAGS = $(LDFLAGS)
PLATFORM_CXXFLAGS =

# Handle jemalloc if enabled
ifndef DISABLE_JEMALLOC
	ifdef JEMALLOC
		PLATFORM_CXXFLAGS += -DROCKSDB_JEMALLOC -DJEMALLOC_NO_DEMANGLE
	endif
	EXEC_LDFLAGS := $(JEMALLOC_LIB) $(EXEC_LDFLAGS) -lpthread
	PLATFORM_CXXFLAGS += $(JEMALLOC_INCLUDE)
endif

# Handle RTTI
ifneq ($(USE_RTTI), 1)
	CXXFLAGS += -fno-rtti
endif

# Source files
SRCS = src/db_env.cc src/working_version.cc src/fluid_lsm.cc src/aux_time.cc src/stats.cc
OBJS = $(SRCS:.cc=.o)

# Output binary name
TARGET = working_version

# RocksDB static library
ROCKSDB_LIB = lib/rocksdb/librocksdb.a

# Phony targets for clean and building RocksDB
.PHONY: all clean librocksdb

# Default target
all: $(TARGET)

# Build the target
$(TARGET): $(OBJS) librocksdb
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(ROCKSDB_LIB) $(EXEC_LDFLAGS)

# Rule to build object files
%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Build RocksDB
librocksdb:
	cd lib/rocksdb && $(MAKE) static_lib

# Clean the build
clean:
	rm -rf $(OBJS) $(TARGET)
	cd lib/rocksdb && $(MAKE) clean
