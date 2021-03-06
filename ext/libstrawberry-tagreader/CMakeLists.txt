cmake_minimum_required(VERSION 3.0)

set(MESSAGES tagreadermessages.proto)
set(SOURCES tagreader.cpp)

protobuf_generate_cpp(PROTO_SOURCES PROTO_HEADERS ${MESSAGES})

link_directories(
  ${GLIB_LIBRARY_DIRS}
  ${PROTOBUF_LIBRARY_DIRS}
  ${TAGLIB_LIBRARY_DIRS}
)

add_library(libstrawberry-tagreader STATIC ${PROTO_SOURCES} ${SOURCES})

target_include_directories(libstrawberry-tagreader SYSTEM PRIVATE
  ${GLIB_INCLUDE_DIRS}
  ${PROTOBUF_INCLUDE_DIRS}
  ${TAGLIB_INCLUDE_DIRS}
)

target_include_directories(libstrawberry-tagreader PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_SOURCE_DIR}/ext/libstrawberry-common
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_BINARY_DIR}/src
)

target_link_libraries(libstrawberry-tagreader PRIVATE
  ${GLIB_LIBRARIES}
  ${PROTOBUF_LIBRARY}
  ${TAGLIB_LIBRARIES}
  ${QtCore_LIBRARIES}
  ${QtNetwork_LIBRARIES}
  libstrawberry-common
)
