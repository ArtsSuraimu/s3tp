get_filename_component(SELF_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)
get_filename_component(S3TP_INCLUDE_DIRS "${SELF_DIR}/../../include/s3tp" ABSOLUTE)
set(S3TP_LIBRARY "${SELF_DIR}/libs3tp.so")
