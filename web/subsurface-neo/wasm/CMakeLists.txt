cmake_minimum_required(VERSION 3.21)

project(SubsurfaceNeoWeb VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(subsurface-neo-web MANUAL_FINALIZATION
	main.cpp
	webcapabilities.cpp
	webcapabilities.h
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

set_target_properties(subsurface-neo-web PROPERTIES
	QT_WASM_INITIAL_MEMORY 134217728
)

qt_finalize_executable(subsurface-neo-web)
