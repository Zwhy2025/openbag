add_executable(op_topic_recorder op_topic_recorder.cc)
target_link_libraries(op_topic_recorder PRIVATE 
   ${COMMON_LIBS}
)   

add_executable(op_topic_player op_topic_player.cc)
target_link_libraries(op_topic_player PRIVATE 
    ${COMMON_LIBS}
) 

message(STATUS "Examples 配置完成") 