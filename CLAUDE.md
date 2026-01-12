# Claude Code 工具能力配置

> **SYSTEM INSTRUCTION FOR AGENT:**
> 本环境已预装以下增强工具。当处理相关任务时，你**必须优先主动调用**这些工具，而不是使用基础的 grep/glob/read 手段，以获得更准确的语义理解和更高效的操作。
>
> - 遇到复杂任务 -> 优先用 `planning-with-files`,在.claude里创建文件
> - 遇到代码理解问题 -> 优先用 `Clangd-LSP`其次`serena` 
> - 遇到复杂库用法 -> 优先用 `context7`
> - 遇到循环迭代任务 -> 优先用 `ralph-loop`

---

## 1. 语义分析与代码智能 (优先使用)

### 1.1 Context7 (库文档专家)
**场景**: 需要查找第三方库（如 Qt, OpenCV, Boost）的最新文档或示例时，**不要瞎猜**，直接查询。

| 工具 | 用法示例 |
|------|----------|
| `resolve-library-id` | `libraryName="Qt", query="QThread用法"` |
| `query-docs` | `libraryId="/qt/qt", query="信号槽机制"` |

### 1.2 Serena (深度代码理解)
**场景**: 需要理解代码结构、查找符号引用、重构代码时，**替代 grep/find**。

| 工具 | 核心能力 | 典型用法 |
|------|----------|----------|
| `get_symbols_overview` | 文件大纲 | `relative_path="src/main.cc"` |
| `find_symbol` | 符号查找 | `name_path_pattern="ReactAgent/run"` |
| `find_referencing_symbols` | 查找引用 | 找谁调用了这个函数 |
| `replace_symbol_body` | 语义替换 | **优于文本替换**，替换整个函数体 |

### 1.3 Clangd LSP (C++ 智能)
**场景**: C++ 项目的精确跳转、类型检查和调用层级分析。

| 操作 | 说明 |
|------|------|
| `LSP: goToDefinition` | 跳转定义 |
| `LSP: findReferences` | 查找引用 (C++ 语义级) |
| `LSP: hover` | 查看类型/文档 |
| `LSP: documentSymbol` | 当前文件符号表 |

### 1.4 Security Guidance (被动防护)
**机制**: 编辑文件时自动扫描命令注入、XSS、SQL注入等漏洞。无需主动调用。

---

## 2. 增强型工作流 Skills

### 2.1 规划与记忆 (Planning)
**规则**: 遇到 3 步以上的复杂任务，**必须**启动规划模式。

| 命令 | 场景 | 行为 |
|------|------|------|
| `/planning-with-files` | 复杂任务/重构/研究 | 自动创建 `task_plan.md` 跟踪进度 |

**记忆系统 (Serena)**:
- `read_memory`: 任务开始前读取 `project_architecture.md` 等记忆
- `write_memory`: 任务结束后将新知识写入记忆文件

### 2.2 循环迭代 (Ralph Loop)
**场景**: "试错-修正"类任务（如：修复编译错误、通过单元测试）。

```bash
/ralph-loop "修复所有编译错误" --max-iterations 10
```

### 2.3 Git 自动化
| 命令 | 说明 |
|------|------|
| `/commit` | 智能生成提交信息并提交 |
| `/commit-push-pr` | 一键 提交 -> 推送 -> PR |
| `/clean_gone` | 清理已删除分支 |

---

## 3. IDE 与调试能力

| 工具 | 用途 |
|------|------|
| `getDiagnostics` | 获取编译器/Linter 报错信息 |
| `executeCode` | 运行 Python 脚本进行临时计算或验证 |

---

## 4. 提交规范 (Reference)

| 前缀 | 含义 |
|------|------|
| `feat:` | 新功能 |
| `fix:` | Bug 修复 |
| `refactor:` | 代码重构 |
| `docs:` | 文档变更 |
| `build:` | 构建系统 |
| `perf:` | 性能优化 |

---
*Environment Configured & Ready*
