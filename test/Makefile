ROOT := ..
BUILD_DIR := $(ROOT)/build
TEST_BUILD := build

CXX ?= g++

CXXFLAGS += -I$(ROOT)/include -I$(BUILD_DIR) -Itest
CXXFLAGS += -std=c++20 -O2 -Wall -Wextra -Wpedantic

SDL_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null)
SDL_LIBS := $(shell sdl2-config --libs 2>/dev/null)
ifeq ($(strip $(SDL_LIBS)),)
  SDL_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
  SDL_LIBS := $(shell pkg-config --libs sdl2 2>/dev/null)
endif

CXXFLAGS += $(SDL_CFLAGS)
LDFLAGS += $(SDL_LIBS) -lpthread

CORE_SRCS := \
	test_framework.cc \
	toy_cpu_executor.cc

TEST_SRCS := \
	$(CORE_SRCS) \
	stdout_capture.cc \
	integration_tests.cc \
	test_main.cc

DISPLAY_SRCS := \
	$(CORE_SRCS) \
	display_demo.cc

TEST_OBJS := $(patsubst %.cc,$(TEST_BUILD)/%.o,$(TEST_SRCS))
DISPLAY_OBJS := $(patsubst %.cc,$(TEST_BUILD)/%.o,$(DISPLAY_SRCS))

TEST_BIN := $(TEST_BUILD)/tests
DISPLAY_BIN := $(TEST_BUILD)/display_demo

.PHONY: all test display clean

all: test

$(BUILD_DIR)/libemulator.a:
	$(MAKE) -C $(ROOT) all

$(TEST_BUILD)/%.o: %.cc
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TEST_BIN): $(BUILD_DIR)/libemulator.a $(TEST_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(TEST_OBJS) $(BUILD_DIR)/libemulator.a -o $@ $(LDFLAGS)

test: $(TEST_BIN)
	cd $(ROOT) && ./test/$(TEST_BIN)

$(DISPLAY_BIN): $(BUILD_DIR)/libemulator.a $(DISPLAY_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(DISPLAY_OBJS) $(BUILD_DIR)/libemulator.a -o $@ $(LDFLAGS)

display: $(DISPLAY_BIN)
	cd $(ROOT) && ./test/$(DISPLAY_BIN)

clean:
	@rm -rf $(TEST_BUILD)
