#include "state_machine.h"
#include "state_machine_rt.h"
#include <stdio.h>

// Test to verify MISRA compliance patterns work correctly

// Forward declarations
static const SM_State TEST_STATE_1;
static const SM_State TEST_STATE_2;

// Test states
static void test_entry(SM_StateMachine *sm, const SM_Event *event) {
    (void)sm;
    (void)event;
    /* do nothing */
}

static const SM_Transition T1[] = {
    {1, &TEST_STATE_2, NULL, NULL, SM_TRANSITION_EXTERNAL}
};

static const SM_State TEST_STATE_1 = {
    .parent = NULL,
    .entryAction = test_entry,
    .exitAction = NULL,
    .transitions = T1,
    .numTransitions = 1,
    .name = "State1"
};

static const SM_State TEST_STATE_2 = {
    .parent = NULL,
    .entryAction = test_entry,
    .exitAction = NULL,
    .transitions = NULL,
    .numTransitions = 0,
    .name = "State2"
};

int main(void)
{
    printf("=== MISRA Compliance Verification Test ===\n\n");
    
    // Test 1: Base state machine functionality
    printf("1. Testing refactored base state machine...\n");
    SM_StateMachine sm;
    const SM_State *path_buffer[4];
    
    SM_Init(&sm, &TEST_STATE_1, path_buffer, 4, NULL, NULL);
    printf("   Initial state: %s\n", SM_GetCurrentStateName(&sm));
    
    // Test SM_IsInState (refactored to remove break)
    bool in_state1 = SM_IsInState(&sm, &TEST_STATE_1);
    bool in_state2 = SM_IsInState(&sm, &TEST_STATE_2);
    printf("   In State1: %s, In State2: %s\n", 
           in_state1 ? "YES" : "NO", 
           in_state2 ? "YES" : "NO");
    
    SM_Reset(&sm);
    printf("   After reset: %s\n", SM_GetCurrentStateName(&sm));
    
    // Test 2: RTT wrapper error handling
    printf("\n2. Testing RTT wrapper error handling...\n");
    SM_RT_Instance rt_sm = {0};
    SM_RT_Result result;
    
    // Test NULL pointer handling
    result = SM_RT_Init(NULL, &TEST_STATE_1, path_buffer, 4, NULL, NULL);
    printf("   NULL pointer test: %s\n", 
           (result == SM_RT_RESULT_ERROR_NULL_PTR) ? "PASS" : "FAIL");
    
    // Test invalid buffer size
    result = SM_RT_Init(&rt_sm, &TEST_STATE_1, path_buffer, 0, NULL, NULL);
    printf("   Invalid buffer size test: %s\n", 
           (result == SM_RT_RESULT_ERROR_INVALID) ? "PASS" : "FAIL");
    
    // Test proper initialization
    result = SM_RT_Init(&rt_sm, &TEST_STATE_1, path_buffer, 4, NULL, NULL);
    printf("   Proper initialization: %s\n", 
           (result == SM_RT_RESULT_SUCCESS) ? "PASS" : "FAIL");
    
    // Test double initialization
    result = SM_RT_Init(&rt_sm, &TEST_STATE_1, path_buffer, 4, NULL, NULL);
    printf("   Double initialization test: %s\n", 
           (result == SM_RT_RESULT_ERROR_ALREADY_INIT) ? "PASS" : "FAIL");
    
    // Test operations without start
    result = SM_RT_PostEventId(&rt_sm, 1, NULL);
    printf("   Operation without start: %s\n", 
           (result == SM_RT_RESULT_ERROR_NOT_STARTED) ? "PASS" : "FAIL");
    
    // Test start
    result = SM_RT_Start(&rt_sm);
    printf("   Start operation: %s\n", 
           (result == SM_RT_RESULT_SUCCESS) ? "PASS" : "FAIL");
    
    // Test operation after start
    result = SM_RT_PostEventId(&rt_sm, 1, NULL);
    printf("   Operation after start: %s\n", 
           (result == SM_RT_RESULT_SUCCESS) ? "PASS" : "FAIL");
    
    printf("\n=== All verification tests completed ===\n");
    return 0;
}

/*
./test_state_machine
=== MISRA Compliance Verification Test ===

1. Testing refactored base state machine...
   Initial state: State1
   In State1: YES, In State2: NO
   After reset: State1

2. Testing RTT wrapper error handling...
   NULL pointer test: PASS
   Invalid buffer size test: PASS
   Proper initialization: PASS
   Double initialization test: PASS
   Operation without start: PASS
   Start operation: PASS
   Operation after start: PASS

=== All verification tests completed ===
*/