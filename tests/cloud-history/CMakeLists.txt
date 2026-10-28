cmake_minimum_required(VERSION 3.16)
project(NeoHistoryProtocol LANGUAGES CXX)

# The protocol depends only on Qt Core/Network. Exercise adversarial provider
# interleavings before the full native application and both test targets build.
set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 REQUIRED COMPONENTS Core Network Test)
get_filename_component(NEO_ROOT "../.." ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
add_executable(TestCloudSyncHistory
    ../testcloudsynchistory.cpp ../testcloudsynchistory.h
    ${NEO_ROOT}/core/cloudsynchistory.cpp ${NEO_ROOT}/core/cloudsynchistory.h
    ${NEO_ROOT}/core/cloudsyncworkspace.cpp ${NEO_ROOT}/core/cloudsyncworkspace.h
    ${NEO_ROOT}/core/cloudsyncworkspacesource.cpp ${NEO_ROOT}/core/cloudsyncworkspacesource.h
    ${NEO_ROOT}/core/cloudsyncworkspacecopy.cpp ${NEO_ROOT}/core/cloudsyncworkspacecopy.h
    ${NEO_ROOT}/core/cloudsyncworkspaceacceptance.cpp ${NEO_ROOT}/core/cloudsyncworkspaceacceptance.h
    ${NEO_ROOT}/core/cloudsyncworkspacebinding.cpp ${NEO_ROOT}/core/cloudsyncworkspacebinding.h
    ${NEO_ROOT}/core/cloudsyncworkspacereceipt.cpp ${NEO_ROOT}/core/cloudsyncworkspacereceipt.h
    ${NEO_ROOT}/core/cloudsyncworkspacereceive.cpp ${NEO_ROOT}/core/cloudsyncworkspacereceive.h
    ${NEO_ROOT}/core/cloudsyncworkspacereceiveplan.cpp ${NEO_ROOT}/core/cloudsyncworkspacereceiveplan.h
    ${NEO_ROOT}/core/cloudsyncworkspaceroute.cpp ${NEO_ROOT}/core/cloudsyncworkspaceroute.h
    ${NEO_ROOT}/core/cloudsyncworkspaceupload.cpp ${NEO_ROOT}/core/cloudsyncworkspaceupload.h
    ${NEO_ROOT}/core/cloudsyncworkspaceread.cpp ${NEO_ROOT}/core/cloudsyncworkspaceread.h
    ${NEO_ROOT}/core/cloudsyncworkspaceapply.cpp ${NEO_ROOT}/core/cloudsyncworkspaceapply.h
    ${NEO_ROOT}/core/cloudsyncfilestore.cpp ${NEO_ROOT}/core/cloudsyncfilestore.h
    ${NEO_ROOT}/core/cloudsyncmanifest.cpp ${NEO_ROOT}/core/cloudsyncmanifest.h
    ${NEO_ROOT}/core/cloudsyncprovider.cpp ${NEO_ROOT}/core/cloudsyncprovider.h)
target_include_directories(TestCloudSyncHistory PRIVATE ${NEO_ROOT})
target_link_libraries(TestCloudSyncHistory PRIVATE Qt6::Core Qt6::Network Qt6::Test)
enable_testing()
add_test(NAME TestCloudSyncHistory COMMAND TestCloudSyncHistory -o "${CMAKE_CURRENT_BINARY_DIR}/history-results.txt,txt")

# Explicit developer tool: no credentials or live calls are part of CTest.
# Its caller verifies the disposable account and newly created fixture IDs.
add_executable(NeoCloudIdentityProbe identityprobe.cpp
	${NEO_ROOT}/core/cloudsynchistory.cpp ${NEO_ROOT}/core/cloudsynchistory.h
	${NEO_ROOT}/core/cloudsyncworkspace.cpp ${NEO_ROOT}/core/cloudsyncworkspace.h
	${NEO_ROOT}/core/cloudsyncworkspacesource.cpp ${NEO_ROOT}/core/cloudsyncworkspacesource.h
	${NEO_ROOT}/core/cloudsyncworkspacecopy.cpp ${NEO_ROOT}/core/cloudsyncworkspacecopy.h
    ${NEO_ROOT}/core/cloudsyncworkspaceacceptance.cpp ${NEO_ROOT}/core/cloudsyncworkspaceacceptance.h
    ${NEO_ROOT}/core/cloudsyncworkspacebinding.cpp ${NEO_ROOT}/core/cloudsyncworkspacebinding.h
    ${NEO_ROOT}/core/cloudsyncworkspacereceipt.cpp ${NEO_ROOT}/core/cloudsyncworkspacereceipt.h
    ${NEO_ROOT}/core/cloudsyncworkspacereceive.cpp ${NEO_ROOT}/core/cloudsyncworkspacereceive.h
    ${NEO_ROOT}/core/cloudsyncworkspacereceiveplan.cpp ${NEO_ROOT}/core/cloudsyncworkspacereceiveplan.h
    ${NEO_ROOT}/core/cloudsyncworkspaceroute.cpp ${NEO_ROOT}/core/cloudsyncworkspaceroute.h
	${NEO_ROOT}/core/cloudsyncworkspaceupload.cpp ${NEO_ROOT}/core/cloudsyncworkspaceupload.h
	${NEO_ROOT}/core/cloudsyncworkspaceread.cpp ${NEO_ROOT}/core/cloudsyncworkspaceread.h
    ${NEO_ROOT}/core/cloudsyncworkspaceapply.cpp ${NEO_ROOT}/core/cloudsyncworkspaceapply.h
    ${NEO_ROOT}/core/cloudsyncfilestore.cpp ${NEO_ROOT}/core/cloudsyncfilestore.h
    ${NEO_ROOT}/core/cloudsyncmanifest.cpp ${NEO_ROOT}/core/cloudsyncmanifest.h
    ${NEO_ROOT}/core/cloudsyncprovider.cpp ${NEO_ROOT}/core/cloudsyncprovider.h)
target_include_directories(NeoCloudIdentityProbe PRIVATE ${NEO_ROOT})
target_link_libraries(NeoCloudIdentityProbe PRIVATE Qt6::Core Qt6::Network)
