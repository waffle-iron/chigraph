cmake_minimum_required(VERSION 2.8)

project(chigraph)

# add the module dirs
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)

set(CG_THIRDPARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/)

option(CG_BUILD_TESTS "Should unit tests be built?" ON)
option(CG_BUILD_GUI "Should the GUI be built" ON)

find_package(Boost REQUIRED system filesystem program_options)

add_subdirectory(third_party/libexecstream)

add_subdirectory(libchig)
add_subdirectory(chig)

if(CG_BUILD_GUI)
	add_subdirectory(third_party/nodeeditor)
	add_subdirectory(gui)
endif()

# Unit tests...
if(CG_BUILD_TESTS)
	enable_testing()
	add_subdirectory(test)
endif()

# Documentation

find_package(Doxygen)

if(DOXYGEN_FOUND)
	
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_BINARY_DIR}/Doxyfile)
	
	add_custom_target(doc
		COMMAND ${DOXYGEN_EXECUTABLE}
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
		COMMENT "Generating Doxygen Docs..."
		DEPENDS ${CHIGRAPH_PRIVATE_FILES} ${CHIGRAPH_PUBLIC_FILES}
	)
	
else()
	message("DOXYGEN not found, target `make doc` will not be avaliable")
endif()

