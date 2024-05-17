
function(ar_check_version)
	string(CONCAT header_content
		"#define SHARED_LIB_API__BUILD_TOOLS_VERSION ${AR_BUILD_TOOLS_VERSION}\n"
		"#define AR_MAJOR_VERSION ${AR_MAJOR_VERSION}\n"
		"#define AR_MINOR_VERSION ${AR_MINOR_VERSION}\n"
		"#define AR_PATCHLEVEL ${AR_PATCHLEVEL}\n"
		"#define AR_VERSION ${AR_VERSION}\n"
	)
	message(STATUS "Generating ${VERSION_H_PATH}")
	file(WRITE "${VERSION_H_PATH}" "${header_content}")
endfunction()

ar_check_version()
