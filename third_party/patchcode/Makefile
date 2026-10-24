# Plain Makefile for patchcode and diffcode.
#
# Usage:
#   make
#   make clean
#   make install PREFIX=/usr/local
#
# Optional Autoconf flow:
#   ./configure --prefix=/usr/local
#   make

-include config.mk

PREFIX ?= /usr/local
bindir ?= $(PREFIX)/bin
BUILD_DIR ?= build/make
CXX ?= c++
EXEEXT ?=
CPPFLAGS ?=
CXXFLAGS ?= -O2 -Wall -Wextra
LDFLAGS ?=
CXXSTD ?= -std=c++11
INSTALL ?= install
MKDIR_P ?= mkdir -p
RM ?= rm -f
RMDIR ?= rm -rf

CPPFLAGS += -Isrc

PATCHCODE_SOURCES := \
	src/main.cpp \
	src/patchcode_tool.cpp \
	src/patchcode_segments.cpp \
	src/patchcode_file.cpp

DIFFCODE_SOURCES := \
	src/diffcode_main.cpp

PATCHCODE_OBJECTS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(PATCHCODE_SOURCES))
DIFFCODE_OBJECTS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(DIFFCODE_SOURCES))

PATCHCODE_BIN := $(BUILD_DIR)/patchcode$(EXEEXT)
DIFFCODE_BIN := $(BUILD_DIR)/diffcode$(EXEEXT)

.PHONY: all clean install uninstall test print-vars

all: $(PATCHCODE_BIN) $(DIFFCODE_BIN)

$(PATCHCODE_BIN): $(PATCHCODE_OBJECTS)
	$(MKDIR_P) $(dir $@)
	$(CXX) $(CXXFLAGS) $(CXXSTD) $(LDFLAGS) -o $@ $^

$(DIFFCODE_BIN): $(DIFFCODE_OBJECTS)
	$(MKDIR_P) $(dir $@)
	$(CXX) $(CXXFLAGS) $(CXXSTD) $(LDFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: %.cpp
	$(MKDIR_P) $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(CXXSTD) -c $< -o $@

install: all
	$(MKDIR_P) $(DESTDIR)$(bindir)
	$(INSTALL) -m 0755 $(PATCHCODE_BIN) $(DESTDIR)$(bindir)/patchcode$(EXEEXT)
	$(INSTALL) -m 0755 $(DIFFCODE_BIN) $(DESTDIR)$(bindir)/diffcode$(EXEEXT)

uninstall:
	$(RM) $(DESTDIR)$(bindir)/patchcode$(EXEEXT)
	$(RM) $(DESTDIR)$(bindir)/diffcode$(EXEEXT)

test: all
	PATH="$(CURDIR)/$(BUILD_DIR):$$PATH" tests/run_diffcode_tests.sh

clean:
	$(RMDIR) $(BUILD_DIR)

print-vars:
	@echo "CXX=$(CXX)"
	@echo "CPPFLAGS=$(CPPFLAGS)"
	@echo "CXXFLAGS=$(CXXFLAGS)"
	@echo "CXXSTD=$(CXXSTD)"
	@echo "LDFLAGS=$(LDFLAGS)"
	@echo "PREFIX=$(PREFIX)"
	@echo "bindir=$(bindir)"
	@echo "EXEEXT=$(EXEEXT)"
