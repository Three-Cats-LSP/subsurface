cmake_minimum_required(VERSION 3.21)

project(SubsurfaceNeoWeb VERSION 0.1 LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
if(BUILD_TESTING)
	find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
endif()
qt_standard_project_setup(REQUIRES 6.8)

set(SUBSURFACE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

qt_add_executable(subsurface-neo-web MANUAL_FINALIZATION
	main.cpp
	neowebdivelogmodel.cpp
	neowebdivelogmodel.h
	webcapabilities.cpp
	webcapabilities.h
	${SUBSURFACE_ROOT}/core/native-divelog-summary.cpp
	${SUBSURFACE_ROOT}/core/native-divelog-summary.h
)

qt_add_qml_module(subsurface-neo-web
	URI SubsurfaceNeo.Web
	VERSION 1.0
	QML_FILES
		Main.qml
)

target_link_libraries(subsurface-neo-web PRIVATE
	Qt6::Core
	Qt6::Gui
	Qt6::Qml
	Qt6::Quick
	Qt6::QuickControls2
)

target_include_directories(subsurface-neo-web PRIVATE ${SUBSURFACE_ROOT})

set_target_properties(subsurface-neo-web PROPERTIES
	QT_WASM_INITIAL_MEMORY 134217728
)

qt_finalize_executable(subsurface-neo-web)

if(BUILD_TESTING AND NOT EMSCRIPTEN)
	qt_add_executable(neo-web-parser-tests
		tests/test_nativedivelogreader.cpp
		neowebdivelogmodel.cpp
		neowebdivelogmodel.h
		${SUBSURFACE_ROOT}/core/native-divelog-summary.cpp
		${SUBSURFACE_ROOT}/core/native-divelog-summary.h
	)
	target_include_directories(neo-web-parser-tests PRIVATE ${SUBSURFACE_ROOT})
	target_link_libraries(neo-web-parser-tests PRIVATE Qt6::Core Qt6::Test)
	add_test(NAME neo-web-parser-tests COMMAND neo-web-parser-tests)
endif()
