set(_QT_ROOT "")

if(QT_ROOT AND IS_DIRECTORY "${QT_ROOT}")
  set(_QT_ROOT "${QT_ROOT}")
  set(_QT_ROOT_EXPLICIT 1)
else()
  set(_ENV_QT_ROOT "")
  if(DEFINED ENV{QT_ROOT})
    file(TO_CMAKE_PATH "$ENV{QT_ROOT}" _ENV_QT_ROOT)
  endif()
  if(_ENV_QT_ROOT AND IS_DIRECTORY "${_ENV_QT_ROOT}")
    set(_QT_ROOT "${_ENV_QT_ROOT}")
    set(_QT_ROOT_EXPLICIT 1)
  endif()
  unset(_ENV_QT_ROOT)
endif()

if(NOT IS_DIRECTORY "${_QT_ROOT}/lib/cmake")
  message(FATAL_ERROR "No CMake bootstrap found for QT binary distribution at: ${_QT_ROOT}.")
endif()

# Execute additional cmake files from the QT binary distribution.
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${_QT_ROOT}/lib/cmake/Qt5Quick")
include("Qt5QuickConfig")
