# Task Plan: 优化语音识别模块

## Goal
提升语音识别模块的可靠性与可维护性：引入更稳健的音频前端/模型管理、优化任务与回调流程，减少误检和锁阻塞，完善配置与文档。

## Current Phase
Phase 4

## Phases

### Phase 1: Requirements & Discovery
- [x] Understand user intent
- [x] Identify constraints and requirements
- [x] Document findings in findings.md
- **Status:** complete

### Phase 2: Planning & Structure
- [x] 确定优化方案：AFE/音频前端选型、任务模型、回调机制
- [x] 规划模型加载与资源清理策略
- [x] 规划配置项与文档更新
- [x] 分析 xiaozhi-esp32 AFE 实现
- **Status:** complete

### Phase 3: Implementation
- [x] 引入 ESP AFE 替代直接喂模型
- [x] 配置 NS (降噪) + VAD
- [x] 重构为 Feed/Fetch 架构
- [x] 添加事件组控制启停
- [x] 编写或更新文档
- [x] 增量自测
- **Status:** complete

### Phase 4: Testing & Verification
- [ ] 在设备上验证唤醒与命令识别可靠性
- [ ] 对比有无 AFE 的识别率
- [ ] 记录测试结果到 progress.md
- [ ] 修复发现的问题
- **Status:** pending

### Phase 5: Delivery
- [ ] 复查改动与文档
- [ ] 确认交付内容完整
- [ ] 向用户汇报
- **Status:** pending

## Key Questions (Answered)
1. ✅ 是否采用 ESP-SR AFE 前端？ → **是，配置 NS + VAD，不启用 AEC**
2. ✅ 模型存储/加载路径？ → 保持 MODEL_IN_FLASH，4M model 分区足够
3. ✅ 语音命令回调？ → 当前同步回调已修复锁问题，暂不改队列

## Decisions Made
| Decision | Rationale |
|----------|-----------|
| 引入 ESP AFE + NS | 提升嘈杂环境识别率，xiaozhi 验证可行 |
| 不启用 AEC | 当前项目无扬声器回放，不需要回声消除 |
| 保持 C 语言 | 与现有代码风格一致，降低复杂度 |
| 添加 VAD 预过滤 | 静音时跳过 WakeNet 检测，省 CPU |

## Implementation Outline

### 新增/修改文件
```
components/sr/
├── afe_processor.c     (新增) AFE 封装
├── afe_processor.h     (新增)
├── voice_recognition.c (修改) 使用 AFE 输出
├── inmp441_driver.c    (保留) I2S 驱动
└── CMakeLists.txt      (修改) 添加 afe_processor
```

### AFE 配置伪代码
```c
afe_config_t* cfg = afe_config_init("M", NULL, AFE_TYPE_VC, AFE_MODE_HIGH_PERF);
cfg->aec_init = false;          // 无扬声器，不需要 AEC
cfg->vad_init = true;           // 启用 VAD
cfg->vad_mode = VAD_MODE_0;
cfg->ns_init = true;            // 启用降噪
cfg->ns_model_name = esp_srmodel_filter(models, ESP_NSNET_PREFIX, NULL);
cfg->afe_ns_mode = AFE_NS_MODE_NET;
cfg->agc_init = false;
cfg->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;

afe_iface = esp_afe_handle_from_config(cfg);
afe_data = afe_iface->create_from_config(cfg);
```

### 任务流程
```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│  I2S Read   │────▶│  AFE Feed   │────▶│ AFE Fetch   │
│ (INMP441)   │     │             │     │ (NS+VAD)    │
└─────────────┘     └─────────────┘     └──────┬──────┘
                                               │
                    ┌──────────────────────────┘
                    ▼
          ┌─────────────────┐
          │ if VAD_SPEECH:  │
          │   WakeNet/MN    │
          │ else:           │
          │   skip detect   │
          └─────────────────┘
```

## Errors Encountered
| Error | Attempt | Resolution |
|-------|---------|------------|
| 锁返回值未检查 | 1 | 添加返回值检查 |
| buzzer 阻塞临界区 | 1 | 移到锁外 |
| SR 模型资源泄漏 | 1 | goto 分级清理 |
| hysteresis 不同步 | 1 | 同步 led_on |
| afe_vad_state_t 重复定义 | 1 | 移除自定义枚举，使用 SDK 类型 |

## Notes
- Update phase status as you progress: pending → in_progress → complete
- Re-read this plan before major decisions (attention manipulation)
- Log ALL errors - they help avoid repetition
- Never repeat a failed action - mutate your approach instead
