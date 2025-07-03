#include "state_machine.h"
#include "state_machine_rtt.h"
#include <stdio.h>

// Test to verify MISRA compliance patterns work correctly

// Forward declarations
static const SM_State TEST_STATE_1;
static const SM_State TEST_STATE_2;

// Test states
static void test_entry(SM_StateMachine *sm, const SM_Event *event) { /* do nothing */ }

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
    Sm_Rtt_Instance rtt_sm = {0};
    Sm_Rtt_Result result;
    
    // Test NULL pointer handling
    result = sm_rtt_init(NULL, &TEST_STATE_1, path_buffer, 4, NULL, NULL);
    printf("   NULL pointer test: %s\n", 
           (result == sm_rtt_result_error_null_ptr) ? "PASS" : "FAIL");
    
    // Test invalid buffer size
    result = sm_rtt_init(&rtt_sm, &TEST_STATE_1, path_buffer, 0, NULL, NULL);
    printf("   Invalid buffer size test: %s\n", 
           (result == sm_rtt_result_error_invalid) ? "PASS" : "FAIL");
    
    // Test proper initialization
    result = sm_rtt_init(&rtt_sm, &TEST_STATE_1, path_buffer, 4, NULL, NULL);
    printf("   Proper initialization: %s\n", 
           (result == sm_rtt_result_success) ? "PASS" : "FAIL");
    
    // Test double initialization
    result = sm_rtt_init(&rtt_sm, &TEST_STATE_1, path_buffer, 4, NULL, NULL);
    printf("   Double initialization test: %s\n", 
           (result == sm_rtt_result_error_already_init) ? "PASS" : "FAIL");
    
    // Test operations without start
    result = sm_rtt_post_event_id(&rtt_sm, 1, NULL);
    printf("   Operation without start: %s\n", 
           (result == sm_rtt_result_error_not_started) ? "PASS" : "FAIL");
    
    // Test start
    result = sm_rtt_start(&rtt_sm);
    printf("   Start operation: %s\n", 
           (result == sm_rtt_result_success) ? "PASS" : "FAIL");
    
    // Test operation after start
    result = sm_rtt_post_event_id(&rtt_sm, 1, NULL);
    printf("   Operation after start: %s\n", 
           (result == sm_rtt_result_success) ? "PASS" : "FAIL");
    
    printf("\n=== All verification tests completed ===\n");
    return 0;
}