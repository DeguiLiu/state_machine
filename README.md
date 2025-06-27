# state_machine

## 1. 介绍

一个轻量级、可移植、支持层级状态的 C 语言状态机框架，适用于嵌入式和Linux应用。

本框架采用数据驱动方式，支持父子状态、入口/出口动作、守卫条件、外部/内部转换、未处理事件钩子等特性。

state_machine的作者是misje, github地址: https://github.com/misje/stateMachine

## 2. 改进点

###  2.1 过程驱动 → 数据驱动

- **改进前**：状态处理分散在各自 handler，需手写 switch-case。
- **改进后**：采用**转换表**（`SM_Transition` 数组）集中描述事件、目标状态、守卫、动作和类型，`SM_Dispatch` 统一查表处理，开发者无需再写繁琐分支。

### 2.2 功能增强

- **守卫/动作**：转换表支持 guard/action 指针。
- **内外转换区分**：支持 `SM_TRANSITION_EXTERNAL`（完整 exit/entry）和 `SM_TRANSITION_INTERNAL`（仅 action，不变状态）。

### 2.3 安全与健壮性

- **消除递归**：entry 动作改为迭代，避免栈溢出。
- **高效 LCA**：最低共同祖先算法优化为 O(N)。
- **用户缓冲区**：路径缓冲由用户提供，内存更安全。
- **未处理事件钩子**：支持 unhandledEventHook，便于调试。

### 2.4 代码质量与规范

- **MISRA C**：重构为单一返回点。



## 3. 特性

- 支持层级（嵌套）状态
- 支持外部/内部转换
- 支持入口/出口/转换动作
- 支持守卫条件
- 支持未处理事件钩子
- 与操作系统无关，适配 RT-Thread、裸机等

## 4. 快速开始

### 4.1. 定义事件

```c
enum
{
    EV_POWER_ON,
    EV_START_TASK,
    EV_TASK_COMPLETE,
    EV_POWER_OFF
};
```

### 4.2. 定义状态与转换

```c
// 前置声明
static const SM_State STATE_Off, STATE_On, STATE_Idle, STATE_Running;

// 转换表
const SM_Transition T_Off[] = {
    {EV_POWER_ON, &STATE_Idle, NULL, NULL, SM_TRANSITION_EXTERNAL}
};
const SM_Transition T_On[] = {
    {EV_POWER_OFF, &STATE_Off, NULL, on_power_off, SM_TRANSITION_EXTERNAL}
};
const SM_Transition T_Idle[] = {
    {EV_START_TASK, &STATE_Running, can_start_task, NULL, SM_TRANSITION_EXTERNAL}
};
const SM_Transition T_Running[] = {
    {EV_TASK_COMPLETE, &STATE_Idle, NULL, on_task_done, SM_TRANSITION_EXTERNAL}
};

// 状态定义
static const SM_State STATE_Off = {
    .parent = NULL,
    .entryAction = NULL,
    .exitAction = NULL,
    .transitions = T_Off,
    .numTransitions = sizeof(T_Off) / sizeof(T_Off[0]),
    .name = "Off"
};
static const SM_State STATE_On = {
    .parent = NULL,
    .entryAction = entry_On,
    .exitAction = exit_On,
    .transitions = T_On,
    .numTransitions = sizeof(T_On) / sizeof(T_On[0]),
    .name = "On"
};
static const SM_State STATE_Idle = {
    .parent = &STATE_On,
    .entryAction = entry_Idle,
    .exitAction = NULL,
    .transitions = T_Idle,
    .numTransitions = sizeof(T_Idle) / sizeof(T_Idle[0]),
    .name = "Idle"
};
static const SM_State STATE_Running = {
    .parent = &STATE_On,
    .entryAction = entry_Running,
    .exitAction = exit_Running,
    .transitions = T_Running,
    .numTransitions = sizeof(T_Running) / sizeof(T_Running[0]),
    .name = "Running"
};
```

### 4.3. 定义动作与守卫

```c
void entry_On(SM_StateMachine *sm, const SM_Event *event) { printf("  (Entry)-> On\n"); }
void exit_On(SM_StateMachine *sm, const SM_Event *event) { printf("  (Exit) -> On\n"); }
void entry_Idle(SM_StateMachine *sm, const SM_Event *event) { printf("    (Entry)-> Idle\n"); }
void entry_Running(SM_StateMachine *sm, const SM_Event *event) { printf("    (Entry)-> Running\n"); }
void exit_Running(SM_StateMachine *sm, const SM_Event *event) { printf("    (Exit) -> Running\n"); }
void on_power_off(SM_StateMachine *sm, const SM_Event *event) { printf("  Action: Shutting down...\n"); }
void on_task_done(SM_StateMachine *sm, const SM_Event *event)
{
    AppData *data = (AppData *)sm->userData;
    data->tasks_completed++;
    printf("  Action: Task finished. Total completed: %d\n", data->tasks_completed);
}
bool can_start_task(SM_StateMachine *sm, const SM_Event *event)
{
    AppData *data = (AppData *)sm->userData;
    printf("  Guard: Checking if tasks completed < 3... (%s)\n", (data->tasks_completed < 3) ? "Yes" : "No");
    return data->tasks_completed < 3;
}
```

### 4.4. 初始化与事件分发

```c
#define MAX_STATE_DEPTH 8
const SM_State *path_buffer[MAX_STATE_DEPTH];
AppData app_data = {0};
SM_StateMachine sm;

SM_Init(&sm, &STATE_Off, path_buffer, MAX_STATE_DEPTH, &app_data, on_unhandled_event);

SM_Event ev_power_on = {EV_POWER_ON, NULL};
SM_Dispatch(&sm, &ev_power_on);
```

### 4.5. 查询状态

```c
printf("Current state: %s\n", SM_GetCurrentStateName(&sm));
printf("Is in state 'On'? %s\n", SM_IsInState(&sm, &STATE_On) ? "Yes" : "No");
```

### 4.6. 重置与反初始化

```c
SM_Reset(&sm);
SM_Deinit(&sm);
```

## 5. API 说明

详见 [state_machine.h](inc/state_machine.h) 注释。

- `void SM_Init(...)`：初始化状态机实例
- `void SM_Deinit(...)`：反初始化
- `void SM_Reset(...)`：重置到初始状态
- `bool SM_Dispatch(...)`：分发事件
- `bool SM_IsInState(...)`：判断当前状态是否为某状态或其子状态
- `const char* SM_GetCurrentStateName(...)`：获取当前状态名

## 6. 示例输出

```
Initial state: Off
Is in state 'On'? No
  (Entry)-> On
    (Entry)-> Idle
Current state: Idle
Is in state 'On'? Yes
Is in state 'Idle'? Yes
--- Unhandled Event Hook: Event 99 received in state 'Idle' ---
  (Exit) -> On
After reset, current state: Off
After deinit, current state name: Unknown
```

## 7. 进阶用法

- 支持父子状态（如 `STATE_Idle`、`STATE_Running` 的父状态为 `STATE_On`）
- 支持守卫条件（guard），可实现条件分支
- 支持未处理事件钩子（unhandledEventHook），便于调试和容错
- 支持用户数据指针（userData），便于携带上下文

## 8. 适用场景

- 嵌入式设备状态管理
- 协议栈状态机
- 复杂业务流程建模
- 需要层级状态的场合

## 9. 许可证

MIT License，详见源码头部。

---

如需更多示例，请参考 `examples/` 目录下的代码。



## 10. Examples

使用示例在 [examples](./examples) 下。
