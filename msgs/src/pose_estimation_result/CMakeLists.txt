cmake_minimum_required(VERSION 2.8)
find_package(Protobuf REQUIRED)

message( STATUS ${msgs} )
message( STATUS ${PROTO_SRCS} )
message( STATUS ${PROTOBUF_LIBRARY} )

set (msgs
  pose_estimation_result.proto
)
PROTOBUF_GENERATE_CPP(PROTO_SRCS PROTO_HDRS ${msgs})
add_library( pose_estimation_result SHARED ${PROTO_SRCS})
target_link_libraries( pose_estimation_result ${PROTOBUF_LIBRARY})
