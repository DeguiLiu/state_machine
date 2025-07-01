#include "state_machine_rt.h"
#include <stdio.h>
#include <string.h>

// --- Test State Definitions ---
enum {
    EV_TEST_1 = 1,
    EV_TEST_2 = 2
};

// Test states
static const SM_State TEST_STATE_A;
static const SM_State TEST_STATE_B;

static void entry_A(SM_StateMachine *sm, const SM_Event *event) { 
    printf("Entry A\n"); 
}

static void entry_B(SM_StateMachine *sm, const SM_Event *event) { 
    printf("Entry B\n"); 
}

static const SM_Transition T_A[] = {
    {EV_TEST_1, &TEST_STATE_B, NULL, NULL, SM_TRANSITION_EXTERNAL}
};

static const SM_Transition T_B[] = {
    {EV_TEST_2, &TEST_STATE_A, NULL, NULL, SM_TRANSITION_EXTERNAL}
};

static const SM_State TEST_STATE_A = {
    .parent = NULL,
    .entryAction = entry_A,
    .exitAction = NULL,
    .transitions = T_A,
    .numTransitions = sizeof(T_A) / sizeof(T_A[0]),
    .name = "StateA"
};

static const SM_State TEST_STATE_B = {
    .parent = NULL,
    .entryAction = entry_B,
    .exitAction = NULL,
    .transitions = T_B,
    .numTransitions = sizeof(T_B) / sizeof(T_B[0]),
    .name = "StateB"
};

int main(void)
{
    SM_RT_Instance rt_sm = {0};
    const SM_State *path_buffer[8];
    SM_RT_Result result;
    bool is_in_state;
    const char *state_name;
    SM_RT_Statistics stats;
    
    printf("=== MISRA-C:2012 Compliant RTT State Machine Test ===\n\n");
    
    // Test initialization
    printf("1. Testing SM_RT_Init...\n");
    result = SM_RT_Init(&rt_sm, &TEST_STATE_A, path_buffer, 8, NULL, NULL);
    printf("   Result: %s\n", (result == SM_RT_RESULT_SUCCESS) ? "SUCCESS" : "FAILED");
    
    // Test getting current state name
    printf("\n2. Testing SM_RT_GetCurrentStateName...\n");
    result = SM_RT_GetCurrentStateName(&rt_sm, &state_name);
    printf("   Result: %s, State: %s\n", 
           (result == SM_RT_RESULT_SUCCESS) ? "SUCCESS" : "FAILED", 
           state_name);
    
    // Test state checking
    printf("\n3. Testing SM_RT_IsInState...\n");
    result = SM_RT_IsInState(&rt_sm, &TEST_STATE_A, &is_in_state);
    printf("   Result: %s, Is in StateA: %s\n", 
           (result == SM_RT_RESULT_SUCCESS) ? "SUCCESS" : "FAILED",
           is_in_state ? "YES" : "NO");
    
    // Test start
    printf("\n4. Testing SM_RT_Start...\n");
    result = SM_RT_Start(&rt_sm);
    printf("   Result: %s\n", (result == SM_RT_RESULT_SUCCESS) ? "SUCCESS" : "FAILED");
    
    // Test event posting
    printf("\n5. Testing SM_RT_PostEventId...\n");
    result = SM_RT_PostEventId(&rt_sm, EV_TEST_1, NULL);
    printf("   Result: %s\n", (result == SM_RT_RESULT_SUCCESS) ? "SUCCESS" : "FAILED");
    
    // Check state after transition
    result = SM_RT_GetCurrentStateName(&rt_sm, &state_name);
    printf("   Current state after event: %s\n", state_name);
    
    // Test statistics
    printf("\n6. Testing SM_RT_GetStatistics...\n");
    result = SM_RT_GetStatistics(&rt_sm, &stats);
    printf("   Result: %s\n", (result == SM_RT_RESULT_SUCCESS) ? "SUCCESS" : "FAILED");
    printf("   Events processed: %u\n", (unsigned)stats.total_events_processed);
    printf("   Events unhandled: %u\n", (unsigned)stats.total_events_unhandled);
    printf("   Total transitions: %u\n", (unsigned)stats.total_transitions);
    
    // Test reset
    printf("\n7. Testing SM_RT_Reset...\n");
    result = SM_RT_Reset(&rt_sm);
    printf("   Result: %s\n", (result == SM_RT_RESULT_SUCCESS) ? "SUCCESS" : "FAILED");
    
    result = SM_RT_GetCurrentStateName(&rt_sm, &state_name);
    printf("   State after reset: %s\n", state_name);
    
    // Test statistics reset
    printf("\n8. Testing SM_RT_ResetStatistics...\n");
    result = SM_RT_ResetStatistics(&rt_sm);
    printf("   Result: %s\n", (result == SM_RT_RESULT_SUCCESS) ? "SUCCESS" : "FAILED");
    
    result = SM_RT_GetStatistics(&rt_sm, &stats);
    printf("   Events processed after reset: %u\n", (unsigned)stats.total_events_processed);
    
    // Test stop
    printf("\n9. Testing SM_RT_Stop...\n");
    result = SM_RT_Stop(&rt_sm);
    printf("   Result: %s\n", (result == SM_RT_RESULT_SUCCESS) ? "SUCCESS" : "FAILED");
    
    // Test error cases
    printf("\n10. Testing error cases...\n");
    
    // Test NULL pointer
    result = SM_RT_Init(NULL, &TEST_STATE_A, path_buffer, 8, NULL, NULL);
    printf("    NULL pointer test: %s\n", 
           (result == SM_RT_RESULT_ERROR_NULL_PTR) ? "PASSED" : "FAILED");
    
    // Test double initialization
    SM_RT_Instance test_sm = {0};
    SM_RT_Init(&test_sm, &TEST_STATE_A, path_buffer, 8, NULL, NULL);
    result = SM_RT_Init(&test_sm, &TEST_STATE_A, path_buffer, 8, NULL, NULL);
    printf("    Double init test: %s\n", 
           (result == SM_RT_RESULT_ERROR_ALREADY_INIT) ? "PASSED" : "FAILED");
    
    printf("\n=== All tests completed ===\n");
    return 0;
}