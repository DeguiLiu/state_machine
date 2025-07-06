# state_machine

## 1. 介绍

一个轻量级、可移植、支持层级状态的 C 语言状态机框架，适用于嵌入式和 Linux 应用。

本框架采用数据驱动方式，支持父子状态、入口/出口动作、守卫条件、外部/内部转换、未处理事件钩子等特性。

state_machine 的作者是 misje, github 地址: https://github.com/misje/stateMachine

---

## 2. 快速开始

### 2.1 POSIX 示例

以 `examples/posix_app.c` 为例，适用于 Linux/Unix 环境：

```c
// 定义事件
typedef enum {
    SM_EVENT_POWER_ON = 1,
    SM_EVENT_POST_STEP_OK,
    // ...其他事件...
} sm_event_t;

// 定义状态、转换表、动作、守卫等
// ...existing code...

// 初始化状态机
SM_StateMachine sm;
const SM_State *path_buffer[8];
sm_system_data_t sys_data = {0};
SM_Init(&sm, &sm_state_off, path_buffer, 8, &sys_data, sm_on_unhandled_event);

// 分发事件
SM_Event ev = {SM_EVENT_POWER_ON, NULL};
SM_Dispatch(&sm, &ev);
```

### 2.2 RT-Thread 示例

以 `examples/rtthread_app.c` 为例，适用于 RT-Thread 嵌入式环境：

```c
// 定义事件、状态、转换表等同 POSIX 示例
// ...existing code...

// 初始化状态机
SM_StateMachine sm;
const SM_State *path_buffer[8];
sm_system_data_t sys_data = {0};
SM_Init(&sm, &sm_state_off, path_buffer, 8, &sys_data, sm_on_unhandled_event);

// 通过消息队列/线程分发事件
SM_Event ev = {SM_EVENT_POWER_ON, NULL};
SM_Dispatch(&sm, &ev);
// 或通过 sm_post_event(event_id, context) 投递到消息队列
```

### 2.3 RT 线程安全状态机（state_machine_rt）

适用于需要线程安全和异步事件分发的场景，API 以 `SM_RT_` 开头：

```c
// 初始化
SM_RT_Instance rt_sm;
SM_RT_Init(&rt_sm, ...);

// 启动工作线程
SM_RT_Start(&rt_sm);

// 投递事件（线程安全/可异步）
SM_Event ev = {EV_POWER_ON, NULL};
SM_RT_PostEvent(&rt_sm, &ev);

// 查询状态
SM_RT_GetCurrentStateName(&rt_sm);
```

---

## 3. API 说明

### 3.1 state_machine.h（基础状态机）

- `void SM_Init(SM_StateMachine*, const SM_State*, const SM_State**, int, void*, SM_UnhandledEventHook)`
- `void SM_Deinit(SM_StateMachine*)`
- `void SM_Reset(SM_StateMachine*)`
- `bool SM_Dispatch(SM_StateMachine*, const SM_Event*)`
- `bool SM_IsInState(SM_StateMachine*, const SM_State*)`
- `const char* SM_GetCurrentStateName(SM_StateMachine*)`

### 3.2 state_machine_rt.h（线程安全/异步状态机）

- `void SM_RT_Init(SM_RT_Instance*, ...);`
- `void SM_RT_Deinit(SM_RT_Instance*);`
- `void SM_RT_Start(SM_RT_Instance*);`
- `void SM_RT_Stop(SM_RT_Instance*);`
- `SM_RT_Result SM_RT_PostEvent(SM_RT_Instance*, const SM_Event*);`
- `const char* SM_RT_GetCurrentStateName(SM_RT_Instance*);`
- `void SM_RT_GetStatistics(SM_RT_Instance*, SM_RT_Statistics*);`
- `void SM_RT_ResetStatistics(SM_RT_Instance*);`

### 3.3 POSIX/RT-Thread 示例接口

- POSIX: `sm_post_event(event_id, context)` 通过消息队列异步投递事件
- RT-Thread: `sm_post_event(event_id, context)` 或 MSH 命令 `sm_event_set <event>`

---

## 4. 示例输出

### POSIX 示例

```text
$ ./posix_app demo
Demo: run a full POST + RUN + ERROR + MAINT + UPGRADE + RESET flow
==> Enter Off
Complex State machine initialized. Initial State: Off

--- Event received: 1, dispatching to state machine ---
<== Exit Off
==> Enter PowerOn
POST: Start self-check sequence.
POST: Step 1 started.
POST: Step 1 ok.
<== Exit PostStep
POST: Step 2 started.
POST: Step 2 failed!
<== Exit PostStep
POST: Retry 1
<== Exit PostRetry
POST: Step 3 started.
POST: All steps done.
<== Exit PostStep
POST: Self-check passed.
<== Exit PostPass
<== Exit PostPass
<== Exit PostPass
System running normally.
Current State: Run

--- Event received: 7, dispatching to state machine ---
System running error! Error count: 1
Current State: RunError

--- Event received: 14, dispatching to state machine ---
<== Exit RunError
Current State: Run

--- Event received: 8, dispatching to state machine ---
<== Exit Run
Enter maintenance mode.
Current State: Maint

--- Event received: 9, dispatching to state machine ---
<== Exit Maint
System running normally.
Current State: Run

--- Event received: 10, dispatching to state machine ---
<== Exit Run
Enter upgrade mode.
Current State: Upgrade

--- Event received: 11, dispatching to state machine ---
Upgrade finished, system will reset.
<== Exit UpgradeDone
<== Exit UpgradeDone
==> Enter Off
Current State: Off

--- Event received: 13, dispatching to state machine ---
--- Unhandled Event: Event 13 received in state 'Off' ---
Current State: Off
```

### RT-Thread 示例

```text
msh > sm_event_set demo
Demo: run a full POST + RUN + ERROR + MAINT + UPGRADE + RESET flow
==> Enter Off
Complex State machine initialized. Initial State: Off
...
```

### RT 线程安全状态机（rt_posix_app）

```text
$ ./rt_posix_app demo
Demo: run a full POST + RUN + ERROR + MAINT + UPGRADE + RESET flow
==> Enter Unknown
Complex RT State machine initialized. Initial State: Off
...
```

---

## 5. SM_Dispatch 与 SM_RT_PostEvent 区别

SM_RT_PostEvent 是为异步和线程安全设计的接口，当前实现和 SM_Dispatch 类似，但结构和扩展性不同。这两个函数的实现区别如下：

### 5.1 SM_Dispatch
- **适用对象**：基础状态机（`SM_StateMachine`）。
- **实现方式**：直接同步分发事件，立即处理。
- **线程安全**：不保证线程安全，通常只用于单线程环境。

### 5.2 SM_RT_PostEvent
- **适用对象**：RTOS/线程安全状态机（`SM_RT_Instance`）。
- **实现方式**：当前实现为同步分发，未来可扩展为异步队列分发。
- **线程安全**：设计上可支持多线程（需配合互斥锁/队列）。

### 代码实现对比（简化版）

```c
// SM_Dispatch
bool SM_Dispatch(SM_StateMachine *sm, const SM_Event *event) {
    // 直接同步处理事件
    // ...existing code...
}

// SM_RT_PostEvent
SM_RT_Result SM_RT_PostEvent(SM_RT_Instance *rt_sm, const SM_Event *event) {
    // 当前实现：直接同步分发
    return dispatch_event_internal(rt_sm, event);
    // 未来可扩展为异步队列分发
}
```

**一句话总结**：  
SM_RT_PostEvent 是为异步和线程安全设计的接口，当前实现和 SM_Dispatch 类似，但结构和扩展性不同。

---

## 6. examples 目录说明

- **posix_app.c**：基于 POSIX（Linux/Unix）接口实现的状态机应用示例，适合 PC 或 Linux 嵌入式环境。
- **rtthread_app.c**：基于 RT-Thread 实时操作系统的状态机用法演示，适合嵌入式开发板。
- **state_machine_example.c**：RT-Thread 下的基础状态机教学/演示示例，通过命令行输入字符触发事件。
- 其他文件：如有 demo、测试或移植相关的示例文件，具体可根据文件名和注释判断其用途。

---

## 7. `state_machine_rtt.c` 相比 `state_machine.c` 主要优势

1. **线程安全设计**  
   预留了互斥锁、消息队列、工作线程等成员，便于在多线程/RTOS环境下安全使用状态机。

2. **异步事件分发能力（可扩展）**  
   结构和接口已为异步事件分发做准备，未来可通过消息队列+线程实现事件异步处理，避免阻塞主线程。

3. **运行统计信息**  
   内置了事件处理总数、未处理事件数、状态切换次数、队列深度等统计，便于系统监控和调优。

4. **更丰富的API**  
   提供了如 `SM_RT_Start`、`SM_RT_Stop`、`SM_RT_GetStatistics`、`SM_RT_ResetStatistics` 等高级接口，方便管理和监控状态机运行。

