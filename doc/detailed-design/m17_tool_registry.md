# M17: ToolRegistry 详细设计

> **版本**: 0.2.0
> **日期**: 2026-05-14
> **对应概要设计**: doc/high-level-design.md §3.18
> **所属层**: CLI Framework

---

## 1. 模块概述

### 1.1 职责

CLI 工具的注册与查找系统。支持静态度注册模式——工具在编译时通过宏自动注册到全局注册表。

### 1.2 文件位置

| 类型 | 路径 |
|------|------|
| 头文件 | `include/libre_bio/cli/tool_registry.h` |
| 实现文件 | `src/cli/tool_registry.cpp` |

### 1.3 依赖

仅 STL。

---

## 2. 数据结构设计

### 2.1 ToolEntry

```cpp
using ToolEntryFunc = int (*)(int argc, const char* argv[]);

struct ToolEntry {
    std::string name;         // 工具名称（子命令名）
    std::string description;  // 工具描述
    std::string version;      // 工具版本
    ToolEntryFunc func;       // 入口函数指针
};
```

### 2.2 ToolRegistry (单例)

```cpp
class ToolRegistry {
private:
    std::map<std::string, ToolEntry> m_tools;  // 工具名 → 条目

    ToolRegistry() = default;  // 私有构造（单例模式）
};
```

### 2.3 静态注册机制

注册通过 `LIBRE_BIO_REGISTER_TOOL` 宏在全局作用域完成：

```cpp
#define LIBRE_BIO_REGISTER_TOOL(name, desc, ver, func) \
    namespace { \
        struct Register_##func { \
            Register_##func() { \
                libre_bio::cli::ToolRegistry::instance() \
                    .register_tool({name, desc, ver, func}); \
            } \
        }; \
        static Register_##func g_register_##func; \
    }
```

工作流程:
1. 每个工具实现文件 (.cpp) 底部调用宏
2. 宏展开生成匿名命名空间内的静态注册器对象
3. C++ 保证静态对象在 `main()` 之前构造
4. 构造器调用 `register_tool` 将工具注册到单例注册表

此模式的优点:
- 添加工具只需在 .cpp 文件中加一行宏，无需修改注册表
- 链接时自动收集所有已链接的工具
- 符合开闭原则 (Open/Closed Principle)

潜在缺陷 (SIOF):
- 静态初始化顺序问题 (Static Initialization Order Fiasco)
- 缓解: ToolRegistry 使用函数内静态变量 (Meyer's Singleton)
  ```cpp
  static ToolRegistry& instance() noexcept {
      static ToolRegistry registry;
      return registry;
  }
  ```

---

## 3. 算法设计

### 3.1 register_tool(entry)

```
输入: entry (const ToolEntry&)
处理:
  1. m_tools[entry.name] = entry
  2. 若 name 已存在 → 覆盖（静默）
输出: void
复杂度: O(log n)
```

### 3.2 find_tool(name)

```
输入: name (const string&)
处理:
  1. 在 m_tools 中查找 name
  2. 找到 → 返回 &value
  3. 未找到 → 返回 nullptr
输出: const ToolEntry*
复杂度: O(log n)
```

### 3.3 list_tools()

```
输入: 无
处理:
  1. 遍历 m_tools，收集所有 ToolEntry* 到 vector
  2. 返回 vector
输出: vector<const ToolEntry*>
复杂度: O(n)
```

---

## 4. 测试设计

### 4.1 测试用例

#### TC01: 注册 + 查找

| 项目 | 内容 |
|------|------|
| 输入 | register_tool({"test", "test tool", "1.0", test_main}) → find_tool("test") |
| 预期 | find_tool 返回非 nullptr, entry.name=="test" |
| 类型 | 正常路径 |

#### TC02: 查找不存在的工具

| 项目 | 内容 |
|------|------|
| 输入 | find_tool("nonexistent") |
| 预期 | 返回 nullptr |
| 类型 | 边界条件 |

#### TC03: 重复注册

| 项目 | 内容 |
|------|------|
| 输入 | 同名 "test" 注册两次（第二次不同 func） |
| 预期 | find_tool 返回第二次注册的 func |
| 类型 | 边界条件 |

#### TC04: list_tools 空注册表

| 项目 | 内容 |
|------|------|
| 输入 | 不注册任何工具 |
| 预期 | list_tools() 返回空 vector |
| 类型 | 边界条件 |

#### TC05: list_tools 多个工具

| 项目 | 内容 |
|------|------|
| 输入 | 注册 3 个工具 |
| 预期 | list_tools().size()==3 |
| 类型 | 正常路径 |

#### TC06: 静态注册宏

| 项目 | 内容 |
|------|------|
| 输入 | 使用 LIBRE_BIO_REGISTER_TOOL 宏注册 → 在 main 中查询 |
| 预期 | find_tool 找到该工具 |
| 类型 | 集成测试 |

---

## 5. MISRA 合规要点

| 规则 | 要点 | 本模块措施 |
|------|------|------------|
| Rule 8.0.1 | enum class | 不适用 |
| Rule 18.0.1 | 禁用异常 | 查找返回 nullptr 而非抛异常 |
| Rule 10.0.3 | const 正确性 | find/list 返回 const 指针 |
