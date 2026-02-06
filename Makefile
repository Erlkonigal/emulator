CC ?= g++
AR ?= ar
PWD = $(shell pwd)
BUILD_DIR = ${PWD}/build
INCLUDE_DIR = ${PWD}/include

CXXFLAGS += -I$(INCLUDE_DIR) -I$(BUILD_DIR)
CXXFLAGS += -std=c++20 -O2 -Wall -fPIC

SDL_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null)
SDL_LDFLAGS := $(shell sdl2-config --libs 2>/dev/null)
CXXFLAGS += $(SDL_CFLAGS)
LDFLAGS += $(SDL_LDFLAGS)

SRCS = $(shell find . -name '*.cpp')
OBJS = $(patsubst ./%.cpp,$(BUILD_DIR)/%.o,$(SRCS))
LIB = $(BUILD_DIR)/libemulator.a

all: $(LIB)

clean:
	@rm -rf build

$(LIB): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	$(AR) rcs $@ $(OBJS)

$(BUILD_DIR)/%.o: ./%.cpp
	@mkdir -p $(dir $@)
	$(CC) $(CXXFLAGS) -c $< -o $@
