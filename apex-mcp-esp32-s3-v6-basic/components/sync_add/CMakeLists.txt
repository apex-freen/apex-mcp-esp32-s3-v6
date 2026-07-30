idf_component_register(SRCS "sync_add.c"
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES cjson apex_cmd_executor) 
# 注意：priv_requires 里面写上需要引用的其他组件名称
