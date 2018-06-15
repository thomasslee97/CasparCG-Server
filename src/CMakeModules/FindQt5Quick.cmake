set(_QT_ROOT "/opt/qt5")

if(NOT IS_DIRECTORY "${_QT_ROOT}/lib/cmake")
  message(FATAL_ERROR "No CMake bootstrap found for QT binary distribution at: ${_QT_ROOT}.")
endif()

# Execute additional cmake files from the CEF binary distribution.
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${_QT_ROOT}/lib/cmake/Qt5Quick")
include("Qt5QuickConfig")
