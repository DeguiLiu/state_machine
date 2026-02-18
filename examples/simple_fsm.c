/*
 * simple_fsm.c - Simple finite state machine example
 *
 * Demonstrates basic state machine usage with transitions
 */

#include <stdio.h>
#include "state_machine/state_machine.h"

typedef enum {
    STATE_IDLE = 0,
    STATE_RUNNING = 1,
    STATE_STOPPED = 2
} SystemState;

typedef struct {
    SystemState state;
    int counter;
} SystemContext;

static void on_enter_idle(void *ctx) {
    SystemContext *c = (SystemContext *)ctx;
    printf("[ENTER] IDLE state\n");
    c->counter = 0;
}

static void on_exit_idle(void *ctx) {
    printf("[EXIT] IDLE state\n");
}

static void on_enter_running(void *ctx) {
    printf("[ENTER] RUNNING state\n");
}

static void on_exit_running(void *ctx) {
    printf("[EXIT] RUNNING state\n");
}

static void on_enter_stopped(void *ctx) {
    printf("[ENTER] STOPPED state\n");
}

static void on_exit_stopped(void *ctx) {
    printf("[EXIT] STOPPED state\n");
}

int main(void) {
    SystemContext ctx = {STATE_IDLE, 0};

    printf("=== Simple FSM Example ===\n");
    printf("Initial state: IDLE\n");

    // Simulate state transitions
    printf("\nTransition: IDLE -> RUNNING\n");
    on_exit_idle(&ctx);
    ctx.state = STATE_RUNNING;
    on_enter_running(&ctx);

    printf("\nTransition: RUNNING -> STOPPED\n");
    on_exit_running(&ctx);
    ctx.state = STATE_STOPPED;
    on_enter_stopped(&ctx);

    printf("\nTransition: STOPPED -> IDLE\n");
    on_exit_stopped(&ctx);
    ctx.state = STATE_IDLE;
    on_enter_idle(&ctx);

    printf("\n=== FSM Example Complete ===\n");
    return 0;
}
