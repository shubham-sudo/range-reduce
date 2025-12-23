
ifndef DISABLE_JEMALLOC
	ifdef JEMALLOC
		PLATFORM_CXXFLAGS += -DROCKSDB_JEMALLOC -DJEMALLOC_NO_DEMANGLE
	endif
	EXEC_LDFLAGS := $(JEMALLOC_LIB) $(EXEC_LDFLAGS) -lpthread
	PLATFORM_CXXFLAGS += $(JEMALLOC_INCLUDE)
endif

ifneq ($(USE_RTTI), 1)
	CXXFLAGS += -fno-rtti
endif

CXX = g++
CXX_FLAGS = -std=c++1y -O0 -ggdb -g3 

all: emu_runner # , load_gen

# CXXFLAGS += -std=c++1y
# $(info PRINT FLAGS $(CXXFLAGS))
# PLATFORM_CXXFLAGS := $(filter-out -std=c++11,$(PLATFORM_CXXFLAGS))
# $(info PRINT FLAGS $(PLATFORM_CXXFLAGS))
# PLATFORM_CXXFLAGS += -std=c++1y
# $(info PRINT FLAGS $(PLATFORM_CXXFLAGS))
# CFLAGS += -std=c++1y
# $(info PRINT FLAGS $(CFLAGS))


emu_runner: emu_runner.cc query_stats.cc emu_environment.cc entry.cc page.cc sstfile.cc vector_memtable.cc tree_builder.cc workload_executor.cc workload_generator.cc query_runner.cc load_gen.cc Generator.cc Key.cc
	$(CXX) $(CXX_FLAGS)  $@.cc -o$@ query_stats.cc emu_environment.cc entry.cc page.cc sstfile.cc vector_memtable.cc tree_builder.cc workload_executor.cc workload_generator.cc query_runner.cc load_gen.cc Generator.cc Key.cc

load_gen: load_gen2.cc Generator.cc Key.cc
	$(CXX) $(CXXFLAGS) -o $@ load_gen2.cc Generator.cc Key.cc -O2 -std=c++11 $(PLATFORM_LDFLAGS) $(PLATFORM_CXXFLAGS) $(EXEC_LDFLAGS)

clean:
	rm -rf emu_runner



