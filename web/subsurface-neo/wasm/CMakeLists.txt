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

set(NEO_PROFILE_CORE_SOURCES
	${SUBSURFACE_ROOT}/core/native-profile-calculator.cpp
	${SUBSURFACE_ROOT}/core/native-profile-calculator.h
	${SUBSURFACE_ROOT}/core/profile.cpp
	${SUBSURFACE_ROOT}/core/profile.h
	${SUBSURFACE_ROOT}/core/profile-ascent.cpp
	${SUBSURFACE_ROOT}/core/profile-deco-context.cpp
	${SUBSURFACE_ROOT}/core/deco.cpp
	${SUBSURFACE_ROOT}/core/deco.h
	${SUBSURFACE_ROOT}/core/gas.cpp
	${SUBSURFACE_ROOT}/core/gas-model.cpp
	${SUBSURFACE_ROOT}/core/gaspressures.cpp
	${SUBSURFACE_ROOT}/core/dive.cpp
	${SUBSURFACE_ROOT}/core/divecomputer.cpp
	${SUBSURFACE_ROOT}/core/equipment.cpp
	${SUBSURFACE_ROOT}/core/event.cpp
	${SUBSURFACE_ROOT}/core/eventtype.cpp
	${SUBSURFACE_ROOT}/core/sample.cpp
	${SUBSURFACE_ROOT}/core/units.cpp
	${SUBSURFACE_ROOT}/core/pref.cpp
	${SUBSURFACE_ROOT}/core/subsurface-string.cpp
	${SUBSURFACE_ROOT}/core/format.cpp
	${SUBSURFACE_ROOT}/core/errorhelper.cpp
	${SUBSURFACE_ROOT}/core/membuffer.cpp
	${SUBSURFACE_ROOT}/core/tag.cpp
	${SUBSURFACE_ROOT}/core/picture.cpp
	${SUBSURFACE_ROOT}/core/trip.cpp
	${SUBSURFACE_ROOT}/core/divesite.cpp
	${SUBSURFACE_ROOT}/core/gettextfromc.cpp
)

add_library(neo-profile-core STATIC ${NEO_PROFILE_CORE_SOURCES})
target_include_directories(neo-profile-core PUBLIC
	${SUBSURFACE_ROOT}
	${SUBSURFACE_ROOT}/libdivecomputer/include
)
target_link_libraries(neo-profile-core PUBLIC Qt6::Core)
if(NOT MSVC)
	target_compile_options(neo-profile-core PRIVATE -ffunction-sections -fdata-sections)
	target_link_options(neo-profile-core INTERFACE -Wl,--gc-sections)
endif()

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
	neo-profile-core
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
	target_link_libraries(neo-web-parser-tests PRIVATE neo-profile-core Qt6::Core Qt6::Test)
	add_test(NAME neo-web-parser-tests COMMAND neo-web-parser-tests)
endif()
