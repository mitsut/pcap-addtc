if(NOT DEFINED PCAP_SUMMARY_EXE)
  message(FATAL_ERROR "PCAP_SUMMARY_EXE is not set")
endif()

if(NOT DEFINED PCAP_FILE)
  message(FATAL_ERROR "PCAP_FILE is not set")
endif()

execute_process(
  COMMAND "${PCAP_SUMMARY_EXE}" "${PCAP_FILE}"
  RESULT_VARIABLE run_result
  OUTPUT_VARIABLE run_output
  ERROR_VARIABLE run_error
)

if(NOT run_result EQUAL 0)
  message(FATAL_ERROR "pcap_summary failed: ${run_error}")
endif()

string(FIND "${run_output}" "Frame count:" has_frame_count)
string(FIND "${run_output}" "First frame time:" has_first)
string(FIND "${run_output}" "Last frame time:" has_last)
string(FIND "${run_output}" "Duration:" has_duration)

if(has_frame_count EQUAL -1 OR has_first EQUAL -1 OR has_last EQUAL -1 OR has_duration EQUAL -1)
  message(FATAL_ERROR "Expected summary fields not found in output:\n${run_output}")
endif()
