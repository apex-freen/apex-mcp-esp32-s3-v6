#pragma once

/**
 * @brief 注册 SD 卡文件操作指令：
 *   - sdList：列出 /sdcard 根目录文件
 *   - sdRead：读取指定文件内容（≤4KB）
 *   - sdWrite：写入/覆盖指定文件
 * 必须在 apex_cmd_executor_init()（即 apex_core_init()）之后调用。
 */
void sd_file_app_init(void);
