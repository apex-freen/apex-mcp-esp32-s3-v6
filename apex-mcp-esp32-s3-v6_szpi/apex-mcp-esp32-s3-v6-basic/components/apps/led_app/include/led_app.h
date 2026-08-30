#pragma once

/**
 * @brief 注册 ledBlink 指令（持久化 + stop 模式示范）
 *
 * 示例：如何实现"持续性动作"——启动后持续闪烁，通过系统 stop 指令终止。
 * 必须在 apex_cmd_executor_init()（即 apex_core_init()）之后调用。
 */
void led_app_init(void);
