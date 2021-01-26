# find or download Assimp

if (${FG_ENABLE_ASSIMP})
	set( FG_EXTERNAL_ASSIMP_PATH "" CACHE PATH "path to Assimp source" )

	# reset to default
	if (NOT EXISTS "${FG_EXTERNAL_ASSIMP_PATH}/include/assimp")
		message( STATUS "Assimp is not found in \"${FG_EXTERNAL_ASSIMP_PATH}\"" )
		set( FG_EXTERNAL_ASSIMP_PATH "${FG_EXTERNALS_PATH}/assimp" CACHE PATH "" FORCE )
		set( FG_ASSIMP_REPOSITORY "https://github.com/assimp/assimp.git" )
	else ()
		set( FG_ASSIMP_REPOSITORY "" )
	endif ()

	ExternalProject_Add( "External.Assimp"
        LIST_SEPARATOR		"${FG_LIST_SEPARATOR}"
		# download
		GIT_REPOSITORY		${FG_ASSIMP_REPOSITORY}
		GIT_TAG				master
		GIT_PROGRESS		1
		EXCLUDE_FROM_ALL	1
		LOG_DOWNLOAD		1
		# update
		PATCH_COMMAND		""
		UPDATE_DISCONNECTED	1
		# configure
        SOURCE_DIR			"${FG_EXTERNAL_ASSIMP_PATH}"
		CMAKE_GENERATOR		"${CMAKE_GENERATOR}"
		CMAKE_GENERATOR_TOOLSET	"${CMAKE_GENERATOR_TOOLSET}"
        CMAKE_ARGS			"-DCMAKE_CONFIGURATION_TYPES=${FG_EXTERNAL_CONFIGURATION_TYPES}"
							"-DCMAKE_SYSTEM_VERSION=${CMAKE_SYSTEM_VERSION}"
							"-DCMAKE_DEBUG_POSTFIX="
							"-DCMAKE_RELEASE_POSTFIX="
							"-DASSIMP_BUILD_ASSIMP_TOOLS=OFF"
							"-DASSIMP_BUILD_ASSIMP_VIEW=OFF"
							"-DASSIMP_BUILD_SAMPLES=OFF"
							"-DASSIMP_BUILD_TESTS=OFF"
							"-DASSIMP_INSTALL_PDB=OFF"
                            ${FG_BUILD_TARGET_FLAGS}
		LOG_CONFIGURE 		1
		# build
		BINARY_DIR			"${CMAKE_BINARY_DIR}/build-Assimp"
		BUILD_COMMAND		"${CMAKE_COMMAND}"
							--build .
							--target ALL_BUILD
							--config $<CONFIG>
		LOG_BUILD 			1
		# install
		INSTALL_DIR 		""
		INSTALL_COMMAND		""
		LOG_INSTALL 		1
		# test
		TEST_COMMAND		""
	)
	
	set_property( TARGET "External.Assimp" PROPERTY FOLDER "External" )
	set( FG_GLOBAL_DEFINITIONS "${FG_GLOBAL_DEFINITIONS}" "FG_ENABLE_ASSIMP" )
endif ()
