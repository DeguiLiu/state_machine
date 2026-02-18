/*
 * traffic_light.c - Traffic light state machine example
 *
 * Demonstrates a realistic traffic light FSM with transitions
 */

#include <stdio.h>
#include <unistd.h>
#include "state_machine/state_machine.h"

typedef enum {
    STATE_RED = 0,
    STATE_YELLOW = 1,
    STATE_GREEN = 2
} TrafficLightState;

typedef struct {
    TrafficLightState state;
    int cycle_count;
} TrafficLightContext;

static void on_enter_red(void *ctx) {
    TrafficLightContext *c = (TrafficLightContext *)ctx;
    printf("[%d] 🔴 RED - Stop! (10 seconds)\n", c->cycle_count);
}

static void on_exit_red(void *ctx) {
    printf("    Exiting RED state\n");
}

static void on_enter_green(void *ctx) {
    TrafficLightContext *c = (TrafficLightContext *)ctx;
    printf("[%d] 🟢 GREEN - Go! (8 seconds)\n", c->cycle_count);
}

static void on_exit_green(void *ctx) {
    printf("    Exiting GREEN state\n");
}

static void on_enter_yellow(void *ctx) {
    printf("    🟡 YELLOW - Prepare to stop (3 seconds)\n");
}

static void on_exit_yellow(void *ctx) {
    printf("    Exiting YELLOW state\n");
}

int main(void) {
    TrafficLightContext ctx = {STATE_RED, 0};

    printf("=== Traffic Light FSM Example ===\n\n");

    // Simulate 3 complete cycles
    for (int cycle = 0; cycle < 3; cycle++) {
        ctx.cycle_count = cycle + 1;

        // RED -> GREEN
        printf("Cycle %d:\n", cycle + 1);
        on_exit_red(&ctx);
        ctx.state = STATE_GREEN;
        on_enter_green(&ctx);
        sleep(1);  // Simulate 8 seconds

        // GREEN -> YELLOW
        on_exit_green(&ctx);
        ctx.state = STATE_YELLOW;
        on_enter_yellow(&ctx);
        sleep(1);  // Simulate 3 seconds

        // YELLOW -> RED
        on_exit_yellow(&ctx);
        ctx.state = STATE_RED;
        on_enter_red(&ctx);
        sleep(1);  // Simulate 10 seconds

        printf("\n");
    }

    printf("=== Traffic Light FSM Complete ===\n");
    return 0;
}
