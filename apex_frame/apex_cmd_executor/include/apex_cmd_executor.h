#pragma once

#include "cJSON.h"

#define APEX_OK 0                   // 正常
#define APEX_ASYNC_OK 1             // 异步响应正常
#define APEX_ERR_PARAM -400         // 指令参数错误
#define APEX_ERR_SYS -500           // 系统错误
#define APEX_ERR_NOT_FOUND -404     // 功能未找到
#define APEX_ERR_STANDBY -401       // 系统已休眠
#define APEX_ERR_BUSY -501          // 系统繁忙/独占中
#define APEX_ERR_NOT_SUPPORTED -406 // 功能不支持 停止等功能
#define APEX_ERR_OVERFLOW -502      // 当前可同时运行的指令已满
#define APEX_ERR_TIMEOUT -503       // 系统指令超时
#define APEX_ERR_DEGRADED -504      // 指令已降级（连续失败自动熔断）
#define APEX_ERR_DUPLICATE -505     // 重复指令（短时间内相同指令多次下发）

// 设备指令状态设计
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// // 1. 指令并发属性枚举
typedef enum
{
    APEX_CMD_FLAG_PARALLEL = 0,      // 并行指令（如：加法、读传感器），不改变设备Idle状态
    APEX_CMD_FLAG_EXCLUSIVE = 1,     // 独占指令（如：OTA升级），执行时锁定系统
    APEX_CMD_FLAG_FORCE = 2,         // 强制指令（如：急停、制动），无视Busy状态强制执行
    APEX_CMD_FLAG_ALWAYS_ALLOWED = 3 // 常驻开放指令（如：状态查询、休眠唤醒），无视Busy和Standby限制
} apex_cmd_flag_t;

// 2. 设备运行状态枚举
typedef enum
{
    APEX_STATE_IDLE = 0,
    APEX_STATE_BUSY = 1
} apex_device_state_t;

// 3. 定义活动指令的快照信息
typedef struct
{
    char cmd_key[32];    // 指令标识 (如 "motor_move")
    char msg_id[64];     // 本次指令的唯一 ID (用于精确匹配)
    bool is_persistent;  // 是否为持久化动作 (由 apex_stop 关掉)
    uint32_t start_tick; // 启动时间戳 (方便计算“已运行时长”)
    bool in_use;         // 槽位占用标志
} apex_active_cmd_t;

// 2. 优化后的状态管理器
#define APEX_MAX_PARALLEL_CMDS 8 // 根据 ESP32 内存压力决定

// 4. 全局状态机结构体
typedef struct
{
    apex_device_state_t current_state;
    char exclusive_cmd[32];
    // ✅ 核心进化：活动指令登记表
    apex_active_cmd_t active_cmds[APEX_MAX_PARALLEL_CMDS];
    char last_cmd[32];
    bool is_standby;
    SemaphoreHandle_t lock;
} apex_state_manager_t;

// 暴露全局实例供外部读取
extern apex_state_manager_t g_apex_state;

// ===================== Handler 返回值说明 =====================
// APEX_OK        ( 0)  同步执行成功，框架自动发送响应并 unlock
// APEX_ASYNC_OK  ( 1)  异步任务已启动，框架保持锁定，业务层需在任务结束时调用 apex_cmd_finish
// APEX_ERR_PARAM (-400) 参数错误，框架自动发送错误响应并 unlock
// APEX_ERR_SYS   (-500) 系统错误，框架自动发送错误响应并 unlock
// ... 其他负值错误码同理
//
// 选择指南：
//   - 同步计算/查询      → return APEX_OK;         （响应即时发出）
//   - 创建了 FreeRTOS 任务 → return APEX_ASYNC_OK;   （任务中调 apex_cmd_finish 收尾）
//   - 持久化动作(电机等)  → return APEX_OK;         （is_persistent=true，框架保持锁定）
// ================================================================

typedef int (*apex_cmd_handler_t)(cJSON *params, const char *msg_id, cJSON **res_data);
typedef int (*apex_stop_handler_t)(cJSON *params, const char *msg_id, cJSON **res_data);

/**
 * @brief 设备主动通知回调（可选）
 *
 * 当模块需要在指令执行过程中主动向服务端推送事件通知时实现此回调。
 * 由业务 handler 内部主动调用 apex_cmd_send_notify() 触发，
 * 消息通过 MQTT notice topic 发送到服务端。
 *
 * 示例：设备复位、电机启动、传感器超阈值等设备端事件。
 *
 * @param func_key  触发通知的指令标识
 * @param event     事件类型 (如 "reset", "motor_started", "temp_alarm")
 * @param data      事件附带数据 (cJSON 对象，函数内部接管并销毁)
 */
typedef void (*apex_notify_handler_t)(const char *func_key, const char *event, cJSON *data);

typedef struct
{
    const char *cmd_key;         // 匹配标识 (如 "led_ctrl")
    const char *function_name;   // 友好展示名称 (如 "灯光控制")
    const char *function_desc;   // 功能详细介绍
    const char *function_params; // 参数定义 (JSON Schema 格式，由 build_function_param_desc_json 生成)
    const char *role;            // 权限角色 (如 "admin", "user", "guest")
    const char *version;         // 版本号 (如 "1.0.2")
    apex_cmd_flag_t flags;       // <--- 新增：指令并发属性
    apex_cmd_handler_t handler;  // 执行函数指针
    /**
     * @brief 是否为持久化动作
     * true:  动作执行后不自动结束（如电机旋转、音乐播放），必须通过 apex_stop 停止。
     * false: 指令执行完即代表动作结束（如获取版本、单次采样）。
     */
    bool is_persistent;
    /**
     * @brief 停止动作的回调钩子
     * 当 is_persistent 为 true 时，此字段必填。
     * 框架在收到 apex_stop 指令时会自动调用此函数。
     */
    apex_stop_handler_t stop_handler;
    /**
     * @brief 设备通知回调（可选）
     * 当模块需要主动向服务端推送设备事件时设置此字段。
     * 业务 handler 内部调用 apex_cmd_send_notify() 触发，消息通过 MQTT notice topic 发送。
     */
    apex_notify_handler_t notify_handler;
} apex_cmd_entry_t;

// 描述"一个参数的元信息"
typedef struct
{
    const char *key;         // 参数名: "add", "adder"...
    const char *type;        // 类型: "int", "float", "bool", "string"
    const char *description; // 参数说明 (可选): "第一个加数"、"风扇模式 cool/heat/fan"

    // 数值范围约束（数值类型用）
    int has_min; // 是否有最小值
    int min_val;
    int has_max;
    int max_val;
    int has_multipleOf; // 步进（输出为 JSON Schema 的 multipleOf）
    int multipleOf_val;

    // 枚举约束（string 类型用）
    const char **enum_vals; // 可选值数组: ["cool", "heat"]...
    int enum_count;         // 枚举值个数

    // 单位（可选，构建时自动合并到 description，不输出为独立字段）
    const char *unit;

    // 默认值（has_default=0 表示必填，自动加入 required 数组）
    int has_default;
    int default_val;
} function_param_desc_t;

// 2. 扔进“安全管道”：解码 -> 解密 -> 分发
// 这里的 apex_process_incoming_cmd 是我们定义的管道入口
void apex_process_incoming_cmd(const char *cipher_text, size_t data_len);

// 注册函数：现在需要传入完整的描述信息
void apex_cmd_register(apex_cmd_entry_t entry);

// 解析并执行
void apex_cmd_executor(const char *json_raw);

// 【核心新功能】查询所有功能列表并返回 JSON 字符串
esp_err_t apex_cmd_executor_init(void);

void apex_cmd_send_response(const char *func_key, const char *msg_id, int status_code, cJSON *res_data);

/**
 * @brief 设备主动通知：推送事件到服务端
 *
 * 模块 handler 内部调用此函数，将设备事件通过 MQTT notice topic 发送到服务端。
 * 消息格式: {"function_key":"...","event":"...","data":{...}}
 * 自动加密后发布。
 *
 * @param func_key  触发通知的指令标识
 * @param event     事件类型 (如 "reset", "motor_started")
 * @param data      事件附带数据 (cJSON 对象，函数内部接管并销毁)
 */
void apex_cmd_send_notify(const char *func_key, const char *event, cJSON *data);

void apex_cmd_send_async_done(const char *msg_id, const char *func_key, int code, cJSON *result_obj);

int apex_state_lock(apex_cmd_flag_t flag, const char *cmd_key, const char *msg_id);

void apex_state_unlock(const char *msg_id);

const apex_cmd_entry_t *apex_cmd_find_entry(const char *cmd_key);

const char *apex_state_get_active_msg_id(const char *cmd_key);

const char *apex_state_get_active_msg_id(const char *cmd_key);

/**
 * @brief 手动终结指令并清理槽位 是开发者手动终结指令，而不是一直持续的指令。
 * @param msg_id 指令 ID
 * @param code 状态码 (0 成功)
 * @param data 返回的业务 JSON 数据 (函数内部会接管内存并在发送后释放)
 */
esp_err_t apex_cmd_finish(const char *msg_id, int code, cJSON *data);

char *build_function_param_desc_json(const function_param_desc_t *params, int count,
                                     char *out_buf, int buf_size);
