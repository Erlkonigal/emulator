# C++ 编码规范

## 命名规范

### 类/结构体/枚举
- 使用大驼峰 (PascalCase)
- 示例: `MemoryBus`, `CpuState`, `MemAccess`

### 常量
- 使用 `k` 前缀 + 大驼峰
- 示例: `kDefaultRomBase`, `kInstructionsPerBatch`

### 成员变量
- 使用 `m_` 前缀 + 小驼峰
- 示例: `mCpu`, `mBus`, `mLastSyncCycle`

### 静态变量
- 使用 `s_` 前缀 + 小驼峰
- 示例: `sInstanceCount`

### 函数/方法
- 使用小驼峰
- 示例: `registerDevice()`, `findMapping()`

### 局部变量
- 使用小驼峰
- 示例: `mapping`, `devices`, `input`

### 函数参数
- 使用小驼峰
- 示例: `access`, `device`, `base`

### 命名空间
- 使用小驼峰
- 示例: `namespace emulator { ... }`

## 代码风格

### 大括号
- 使用 K&R 风格 (同行开括号)
- 空函数体: `void foo() {}`

### 缩进
- 使用 4 空格缩进

### 行长度
- 建议不超过 100 字符

### const 正确性
- 成员函数应标记为 `const` 当不修改成员变量时
- 指针参数应明确 `const` 修饰

## C++20 特性

- 优先使用 `std::format` 进行字符串格式化
- 使用 `constexpr` 定义编译期常量
- 使用 `[[nodiscard]]`, `[[maybe_unused]]` 等属性

## 文件结构

- 头文件: `.h` 或 `.hpp`
- 源文件: `.cpp`
- 头文件应包含头保护符 `#ifndef ... / #define ... / #endif`
