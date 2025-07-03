# Makefile for state_machine examples
# Compatible with Linux gcc/make build system

# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -I../inc
LDFLAGS = 

# Source directories
SRCDIR = ../src
INCDIR = ../inc

# Common source files
COMMON_SRCS = $(SRCDIR)/state_machine.c $(SRCDIR)/state_machine_rtt.c

# Target executables
TARGETS = test_rtt_compliance test_misra_verification test_state_machine

# Default target
all: $(TARGETS)

# Individual targets
test_rtt_compliance: test_rtt_compliance.c $(COMMON_SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test_misra_verification: test_misra_verification.c $(COMMON_SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test_state_machine: test_state_machine.c $(SRCDIR)/state_machine.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Clean target
clean:
	rm -f $(TARGETS)

# Run all tests
test: $(TARGETS)
	@echo "Running test_rtt_compliance..."
	./test_rtt_compliance
	@echo
	@echo "Running test_misra_verification..."
	./test_misra_verification
	@echo
	@echo "Running test_state_machine..."
	./test_state_machine

# Help target
help:
	@echo "Available targets:"
	@echo "  all                    - Build all test executables"
	@echo "  test_rtt_compliance    - Build RTT compliance test"
	@echo "  test_misra_verification - Build MISRA verification test"
	@echo "  test_state_machine     - Build basic state machine test"
	@echo "  clean                  - Remove all built executables"
	@echo "  test                   - Build and run all tests"
	@echo "  help                   - Show this help message"

.PHONY: all clean test help