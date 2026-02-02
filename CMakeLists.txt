cmake_minimum_required(VERSION 3.16)

project(pcap_addtc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Try finding libpcap
find_library(PCAP_LIBRARY pcap)
find_path(PCAP_INCLUDE_DIR pcap/pcap.h)

if(NOT PCAP_LIBRARY OR NOT PCAP_INCLUDE_DIR)
  message(FATAL_ERROR "libpcap not found. Please install libpcap development files.")
endif()

add_executable(pcap_addtc main.cpp)
target_link_libraries(pcap_addtc PRIVATE ${PCAP_LIBRARY})
target_include_directories(pcap_addtc PRIVATE ${PCAP_INCLUDE_DIR})

enable_testing()

add_test(
  NAME pcap_addtc_runs
  COMMAND pcap_addtc ${CMAKE_CURRENT_LIST_DIR}/rmap_write.pcap
)

add_test(
  NAME pcap_addtc_output_check
  COMMAND ${CMAKE_COMMAND}
    -DPCAP_SUMMARY_EXE=$<TARGET_FILE:pcap_addtc>
    -DPCAP_FILE=${CMAKE_CURRENT_LIST_DIR}/rmap_write.pcap
    -P ${CMAKE_CURRENT_LIST_DIR}/cmake/CheckPcapOutput.cmake
)
