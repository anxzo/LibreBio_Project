# M18: Logger 详细设计

> **版本**: 0.3.0
> **日期**: 2026-05-15
> **对应概要设计**: doc/high-level-design.md §3.19
> **所属层**: CLI Framework

---

## 1. 模块概述

### 1.1 职责

统一日志输出与进度显示。日志输出到 stderr，数据输出到 stdout。支持多级别日志过滤和 TTY 感知的进度条。

### 1.2 文件位置

| 类型 | 路径 |
|------|------|
| 头文件 | `include/libre_bio/cli/logger.h` |
| 实现文件 | `src/cli/logger.cpp` |

### 1.3 依赖

仅 STL。

---

## 2. 数据结构设计

### 2.1 日志级别

```cpp
enum class LogLevel : uint8_t {
    kError   = 0,  // 严重错误（始终输出）
    kWarning = 1,  // 警告
    kInfo    = 2,  // 一般信息
    kDebug   = 3,  // 调试信息（最详细）
};
```

级别过滤规则: 仅输出级别 ≤ 当前设置级别的消息。
例如: set_level(kWarning) → 输出 error + warning，不输出 info/debug。

### 2.2 Logger 单例

```cpp
class Logger {
public:
    void set_level(LogLevel level) noexcept;
    [[nodiscard]] LogLevel level() const noexcept;

    void error(const std::string& msg);
    void warning(const std::string& msg);
    void info(const std::string& msg);
    void debug(const std::string& msg);

    void progress(uint64_t current, uint64_t total,
                  const std::string& label = "");

    // 测试支持接口
    void set_ostream(std::ostream& os) noexcept;
    void set_tty(bool is_tty) noexcept;
    void reset_progress_state() noexcept;

private:
    LogLevel m_level;                 // 当前日志级别
    bool m_is_tty;                    // stderr 是否为终端
    std::ostream* m_os;               // 输出流指针（默认 &std::cerr）
    uint64_t m_last_reported_percent; // 非 TTY 上次报告百分比

    Logger();                          // 私有构造，检测 isatty(stderr_fileno)
};
```

### 2.3 测试支持接口（v0.3.0 新增）

| 方法 | 说明 |
|------|------|
| set_ostream(os) | 替换输出流。默认 &std::cerr，测试中替换为 std::ostringstream |
| set_tty(is_tty) | 覆盖 TTY 自动检测结果，模拟终端/非终端场景 |
| reset_progress_state() | 重置进度条内部状态（m_last_reported_percent = 0），用于测试隔离 |

### 2.4 便捷宏

```cpp
#define LIBRE_BIO_LOG_ERROR(msg)   Logger::instance().error(msg)
#define LIBRE_BIO_LOG_WARNING(msg) Logger::instance().warning(msg)
#define LIBRE_BIO_LOG_INFO(msg)    Logger::instance().info(msg)

#ifdef NDEBUG
#define LIBRE_BIO_LOG_DEBUG(msg)   ((void)0)
#else
#define LIBRE_BIO_LOG_DEBUG(msg)   Logger::instance().debug(msg)
#endif
```

宏允许编译期零开销移除日志（NDEBUG 下 debug 宏展开为空操作）。

---

## 3. 算法设计

### 3.1 日志输出

```
error(msg) / warning(msg) / info(msg) / debug(msg):
  1. 若对应级别 > m_level → 直接返回（过滤）
  2. 格式化输出: "[LEVEL] message\n"
     例如: "[ERROR] 文件打开失败: input.fa\n"
  3. 写入 std::cerr（stderr）
  4. std::cerr.flush()

复杂度: O(msg.length())
```

### 3.2 TTY 检测

```
Logger 构造函数:
  1. 使用 isatty(STDERR_FILENO) 检测 m_is_tty
  2. 若 isatty 不可用 (Windows) → m_is_tty = false
  3. 默认级别: LogLevel::kInfo
```

### 3.3 progress(current, total, label) — 进度显示

```
输入: current (uint64_t), total (uint64_t), label (string, 可选)
处理:
  1. 若 m_is_tty:
     使用 '\r' (回车) 在原地刷新进度条
     格式: "[label] ████████░░░░░░░░ 50% (5000/10000)"
     进度条宽度: 固定 40 字符（或根据终端宽度动态计算）
     每个进度块: '█' = 完全完成, '░' = 未完成
     百分比: (current * 100) / total
     输出到 std::cerr (无 '\n', 使用 '\r')
     std::cerr.flush()
  
  2. 若非 TTY:
     每 10% 输出一行
     static 变量 last_reported_percent
     if (current_percent / 10 > last_reported_percent / 10):
        输出: "[label] 50% (5000/10000)\n"
        last_reported_percent = current_percent

  3. 若 current == total:
     输出: "[label] 完成\n" (TTY 模式也用 '\n' 换行)

复杂度: O(1)
```

---

## 4. 错误处理

| 场景 | 处理方式 |
|------|----------|
| isatty 失败 | m_is_tty = false，回退到非 TTY 模式 |
| total == 0 | 不输出进度（避免除零） |
| cerr 写入失败 | 不检测（标准错误流），静默失败 |

---

## 5. 测试设计

### 5.1 测试用例

#### TC01: 基本日志输出

| 项目 | 内容 |
|------|------|
| 输入 | error("test error"), warning("test warning"), info("test info") |
| 预期 | 所有三条输出到 stderr（默认级别 kInfo 下） |
| 类型 | 正常路径 |

#### TC02: 日志级别过滤

| 项目 | 内容 |
|------|------|
| 输入 | set_level(kWarning) → debug("debug msg"), info("info msg"), error("error msg") |
| 预期 | error 和 warning 输出，debug 和 info 不输出 |
| 类型 | 正常路径 |

#### TC03: 进度条 TTY 模式带标签

| 项目 | 内容 |
|------|------|
| 输入 | progress(50, 100, "处理中")，模拟 m_is_tty=true |
| 预期 | 输出包含 \\r、[处理中]、50%、(50/100)，不包含 \\n |
| 类型 | 正常路径 |

#### TC03b: 进度条 TTY 模式不带标签

| 项目 | 内容 |
|------|------|
| 输入 | progress(30, 100)，模拟 m_is_tty=true |
| 预期 | 输出包含 \\r、30%、(30/100)，无标签前缀 |
| 类型 | 正常路径 |

#### TC04: 进度条非 TTY 模式

| 项目 | 内容 |
|------|------|
| 输入 | 模拟 m_is_tty=false，progress(10, 100) |
| 预期 | 输出 "10% (10/100)\\n" |
| 类型 | 正常路径 |

#### TC04b: 同一 10% 区间不重复输出

| 项目 | 内容 |
|------|------|
| 输入 | progress(10, 100) → progress(15, 100) |
| 预期 | 第二次调用不产生输出（仍在 10~19% 区间内） |
| 类型 | 边界条件 |

#### TC04c: 超过 10% 阈值再次输出

| 项目 | 内容 |
|------|------|
| 输入 | progress(10, 100) → progress(20, 100) |
| 预期 | 第二次调用输出 "20% (20/100)\\n" |
| 类型 | 正常路径 |

#### TC05: 进度条完成 TTY

| 项目 | 内容 |
|------|------|
| 输入 | progress(100, 100)，模拟 m_is_tty=true |
| 预期 | 输出包含 "完成" 和 \\n（TTY 模式也换行） |
| 类型 | 边界条件 |

#### TC05b: 进度条完成非 TTY 带标签

| 项目 | 内容 |
|------|------|
| 输入 | progress(100, 100, "测试")，模拟 m_is_tty=false |
| 预期 | 输出 "[测试] 完成\\n" |
| 类型 | 边界条件 |

#### TC06: 零总量进度

| 项目 | 内容 |
|------|------|
| 输入 | progress(0, 0, "") |
| 预期 | 不输出，不崩溃 |
| 类型 | 边界条件 |

#### TC07: set_level / level 读写

| 项目 | 内容 |
|------|------|
| 输入 | level() → set_level(kDebug) → level() → set_level(kError) → level() |
| 预期 | 返回值分别为 kInfo、kDebug、kError |
| 类型 | 正常路径 |

#### TC08: 便捷宏

| 项目 | 内容 |
|------|------|
| 输入 | LIBRE_BIO_LOG_ERROR/LOG_WARNING/LOG_INFO 宏调用 |
| 预期 | 等价于直接调用 Logger 实例方法 |
| 类型 | 正常路径 |

#### TC09: kDebug 级别可见

| 项目 | 内容 |
|------|------|
| 输入 | set_level(kDebug) → debug/info/error 依次调用 |
| 预期 | 所有三级消息均输出 |
| 类型 | 正常路径 |

#### TC10: kError 级别 — 仅 error 可见

| 项目 | 内容 |
|------|------|
| 输入 | set_level(kError) → debug/info/warning/error 依次调用 |
| 预期 | 仅 error 输出，其余三条不输出 |
| 类型 | 边界条件 |

#### TC11: 进度条完整序列 TTY

| 项目 | 内容 |
|------|------|
| 输入 | progress(0, 100) → progress(50, 100) → progress(100, 100) |
| 预期 | 最终输出包含 "完成" 和 \\r |
| 类型 | 正常路径 |

---

## 6. MISRA 合规要点

| 规则 | 要点 | 本模块措施 |
|------|------|------------|
| Rule 8.0.1 | enum class | LogLevel 使用 enum class |
| Rule 5.0.1 | 避免 magic number | 进度条宽度、百分比阈值使用 constexpr |
| Rule 8.0.2 | 禁止隐式转换 | 百分比计算显式使用 double |
| Rule 18.0.1 | 禁用异常 | isatty 失败静默处理 |
