# state_machine

[![CI](https://github.com/DeguiLiu/state_machine/actions/workflows/ci.yml/badge.svg)](https://github.com/DeguiLiu/state_machine/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

一个轻量级、可移植、支持层级状态的 C 语言状态机框架，适用于嵌入式和 Linux 应用。

本框架采用数据驱动方式，支持父子状态、入口/出口动作、守卫条件、外部/内部转换、未处理事件钩子等特性。

state_machine 的作者是 misje, github 地址: https://github.com/misje/stateMachine

## 项目结构

```
include/state_machine/    -- 公共头文件
src/                      -- 核心实现
tests/                    -- 单元测试
examples/                 -- 示例程序
docs/                     -- 设计文档
CMakeLists.txt           -- CMake 构建配置
.clang-format            -- 代码格式化配置
```

## 快速开始

### 构建

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . -j$(nproc)
ctest --output-on-failure
```

### 示例程序

#### 简单 HSM (simple_hsm.c)

演示基本的分层状态转换和入口/出口动作。

```bash
./simple_hsm
```

#### 交通灯 HSM (traffic_light.c)

演示实际应用场景：红绿灯分层状态机。

```bash
./traffic_light
```

## 核心 API

### 基础状态机 (state_machine.h)

```c
void SM_Init(SM_StateMachine*, const SM_State*, const SM_State**, int, void*, SM_UnhandledEventHook);
void SM_Deinit(SM_StateMachine*);
void SM_Reset(SM_StateMachine*);
bool SM_Dispatch(SM_StateMachine*, const SM_Event*);
bool SM_IsInState(SM_StateMachine*, const SM_State*);
const char* SM_GetCurrentStateName(SM_StateMachine*);
```

### 线程安全状态机 (state_machine_rt.h)

```c
void SM_RT_Init(SM_RT_Instance*, ...);
void SM_RT_Deinit(SM_RT_Instance*);
void SM_RT_Start(SM_RT_Instance*);
void SM_RT_Stop(SM_RT_Instance*);
SM_RT_Result SM_RT_PostEvent(SM_RT_Instance*, const SM_Event*);
const char* SM_RT_GetCurrentStateName(SM_RT_Instance*);
void SM_RT_GetStatistics(SM_RT_Instance*, SM_RT_Statistics*);
void SM_RT_ResetStatistics(SM_RT_Instance*);
```

## 特性

- **轻量级设计**：最小化内存占用，适合嵌入式系统
- **层级状态机**：支持父子状态和复杂状态转换
- **数据驱动**：通过状态表定义状态机行为
- **线程安全**：state_machine_rt 提供 RTOS 友好的接口
- **可移植性**：支持 POSIX 和 RT-Thread 环境
- **MISRA C 合规**：符合工业级代码标准

## 单元测试

项目包含三个测试程序：

- `state_machine_test` - 基础状态机测试
- `rt_compliance_test` - RT 线程安全测试
- `misra_verification` - MISRA C 合规性验证

所有测试通过 `ctest` 运行：

```bash
cd build && ctest --output-on-failure
```

## 代码质量

项目遵循以下代码质量标准：

- clang-format 代码格式化
- cpplint 代码检查
- cppcheck 静态分析

运行代码质量检查：

```bash
cmake --build build --target format
cmake --build build --target cppcheck
```

## 许可证

MIT License - 详见 LICENSE 文件
