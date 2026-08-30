#pragma once

/**
 * @brief 注册 attitudeGet 指令：读取 QMI8658 六轴姿态（加速度/陀螺仪/倾角）
 *
 * 示例：如何在 components 应用层把"外设能力"暴露为 MCP 工具。
 * 必须在 apex_cmd_executor_init()（即 apex_core_init()）之后调用。
 */
void attitude_app_init(void);
