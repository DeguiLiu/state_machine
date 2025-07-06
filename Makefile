# Makefile for State Machine Library and Examples
# Compatible with Linux/POSIX systems

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pedantic -I./inc
LDFLAGS = -lpthread -lrt

# Directories
SRCDIR = src
INCDIR = inc
EXAMPLEDIR = examples
BUILDDIR = build

# Source files
BASE_SOURCES = $(SRCDIR)/state_machine.c
RT_SOURCES = $(SRCDIR)/state_machine_rt.c
ALL_SOURCES = $(BASE_SOURCES) $(RT_SOURCES)

# Object files
BASE_OBJECTS = $(BUILDDIR)/state_machine.o
RT_OBJECTS = $(BUILDDIR)/state_machine_rt.o
ALL_OBJECTS = $(BASE_OBJECTS) $(RT_OBJECTS)

# Linux-compatible examples (excluding RT-Thread specific ones)
EXAMPLES = test_state_machine test_rt_compliance test_misra_verification posix_async_example posix_app

# Default target
all: $(EXAMPLES)

# Create build directory
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# Compile object files
$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Base state machine library tests (using only state_machine.c)
test_state_machine: $(BASE_OBJECTS) $(EXAMPLEDIR)/test_state_machine.c
	$(CC) $(CFLAGS) -o $@ $(BASE_OBJECTS) $(EXAMPLEDIR)/test_state_machine.c

# RT state machine tests (using state_machine_rt.c independently)
test_rt_compliance: $(RT_OBJECTS) $(EXAMPLEDIR)/test_rt_compliance.c
	$(CC) $(CFLAGS) -o $@ $(RT_OBJECTS) $(EXAMPLEDIR)/test_rt_compliance.c

# MISRA verification test (using both modules together)
test_misra_verification: $(ALL_OBJECTS) $(EXAMPLEDIR)/test_misra_verification.c
	$(CC) $(CFLAGS) -o $@ $(ALL_OBJECTS) $(EXAMPLEDIR)/test_misra_verification.c

# POSIX async example (RT module with pthread/mqueue)
posix_async_example: $(RT_OBJECTS) $(EXAMPLEDIR)/posix_async_example.c
	$(CC) $(CFLAGS) -o $@ $(RT_OBJECTS) $(EXAMPLEDIR)/posix_async_example.c $(LDFLAGS)

# POSIX async example (RT module with pthread/mqueue)
posix_app: $(RT_OBJECTS) $(EXAMPLEDIR)/posix_app.c
	$(CC) $(CFLAGS) -o $@ $(RT_OBJECTS) $(EXAMPLEDIR)/posix_app.c $(LDFLAGS)

# Static library targets
libstatemachine.a: $(BASE_OBJECTS)
	ar rcs $@ $^

libstatemachine_rt.a: $(RT_OBJECTS)
	ar rcs $@ $^

# Combined static library
libstatemachine_all.a: $(ALL_OBJECTS)
	ar rcs $@ $^

# Install targets
install: libstatemachine.a libstatemachine_rt.a
	install -d /usr/local/lib
	install -d /usr/local/include
	install -m 644 libstatemachine.a /usr/local/lib/
	install -m 644 libstatemachine_rt.a /usr/local/lib/
	install -m 644 $(INCDIR)/*.h /usr/local/include/

# Test targets
test: $(EXAMPLES)
	@echo "Running test_state_machine..."
	./test_state_machine
	@echo ""
	@echo "Running test_rt_compliance..."
	./test_rt_compliance
	@echo ""
	@echo "Running test_misra_verification..."
	./test_misra_verification

# Compile check target
check: $(ALL_OBJECTS)
	@echo "All source files compiled successfully!"

# Clean target
clean:
	rm -rf $(BUILDDIR)
	rm -f $(EXAMPLES)
	rm -f *.a

# Help target
help:
	@echo "Available targets:"
	@echo "  all                     - Build all Linux-compatible examples"
	@echo "  test_state_machine      - Build base state machine test"
	@echo "  test_rt_compliance      - Build RT state machine test"
	@echo "  test_misra_verification - Build MISRA compliance test"
	@echo "  posix_async_example     - Build POSIX async example"
	@echo "  libstatemachine.a       - Build base state machine static library"
	@echo "  libstatemachine_rt.a    - Build RT state machine static library"
	@echo "  libstatemachine_all.a   - Build combined static library"
	@echo "  test                    - Build and run all tests"
	@echo "  check                   - Compile check all source files"
	@echo "  install                 - Install libraries and headers"
	@echo "  clean                   - Remove build artifacts"
	@echo "  help                    - Show this help message"
	@echo ""
	@echo "Examples excluded from Linux build (RT-Thread specific):"
	@echo "  - app_state_machine.c"
	@echo "  - post_state.c"
	@echo "  - rtthread_async_example.c"
	@echo "  - state_machine_example.c"

.PHONY: all test check clean install help