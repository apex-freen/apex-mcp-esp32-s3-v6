#define TAG "ATTITUDE_APP"
#include "esp_log.h"
#include "cJSON.h"
#include "apex_cmd_executor.h"
#include "bsp_board.h"
#include "attitude_app.h"

static const char *FUNCTION_KEY = "attitudeGet";

// ==================== 同步 Handler：读姿态 ====================
static int attitude_get_handler(cJSON *params, const char *msg_id, cJSON **res_data)
{
    bsp_imu_data_t imu;
    if (bsp_imu_read(&imu) != ESP_OK)
    {
        ESP_LOGE(TAG, "读取 IMU 失败");
        return APEX_ERR_SYS;
    }

    cJSON *data = cJSON_CreateObject();
    if (data == NULL)
        return APEX_ERR_SYS;

    cJSON_AddNumberToObject(data, "acc_x", imu.acc_x);
    cJSON_AddNumberToObject(data, "acc_y", imu.acc_y);
    cJSON_AddNumberToObject(data, "acc_z", imu.acc_z);
    cJSON_AddNumberToObject(data, "gyr_x", imu.gyr_x);
    cJSON_AddNumberToObject(data, "gyr_y", imu.gyr_y);
    cJSON_AddNumberToObject(data, "gyr_z", imu.gyr_z);
    cJSON_AddNumberToObject(data, "angle_x", imu.AngleX);
    cJSON_AddNumberToObject(data, "angle_y", imu.AngleY);
    cJSON_AddNumberToObject(data, "angle_z", imu.AngleZ);

    *res_data = data;
    return APEX_OK; // 同步指令，框架自动发响应
}

// ==================== 组件注册入口 ====================
void attitude_app_init(void)
{
    apex_cmd_entry_t entry = {
        .cmd_key = FUNCTION_KEY,
        .function_name = "姿态传感器",
        .function_desc = "读取 QMI8658 六轴姿态数据：XYZ 加速度、陀螺仪原始值与倾角（度）",
        .function_params = "{\"type\":\"object\",\"properties\":{}}", // 无参
        .role = "user",
        .risk_level = "normal",
        .version = "1.0.0",
        .flags = APEX_CMD_FLAG_ALWAYS_ALLOWED, // 只读查询，任何状态可执行
        .handler = attitude_get_handler,
        .is_persistent = false,
        .stop_handler = NULL,
    };
    apex_cmd_register(entry);
    ESP_LOGI(TAG, "组件注册成功: %s (v%s)", entry.cmd_key, entry.version);
}
