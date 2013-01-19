cmake_minimum_required(VERSION 2.6)
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
project(pspmaps)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR})
set(CMAKE_CURRENT_SOURCE_DIR ${PROJECT_SOURCE_DIR})
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/Modules/)
set(
   SOURCES
   global.c
   kml.c
   pspmaps.c
)
Find_Package(SDL REQUIRED)
Find_Package(SDL_image REQUIRED)
Find_Package(SDL_gfx REQUIRED)
Find_Package(SDL_ttf REQUIRED)
Find_Package(SDL_mixer REQUIRED)
Find_Package(LibXml2 REQUIRED)
Find_Package(CURL REQUIRED)
IF(UNIX)
   SET(MY_MATH_LIB -lm)
ELSE(UNIX)
   SET(MY_MATH_LIB)
ENDIF(UNIX)
if(NOT SDL_FOUND)
   message(FATAL_ERROR "SDL not found!")
endif(NOT SDL_FOUND)
if(NOT SDLIMAGE_FOUND)
   message(FATAL_ERROR "SDL_image not found!")
endif(NOT SDLIMAGE_FOUND)
if(NOT SDLGFX_FOUND)
   message(FATAL_ERROR "SDL_gfx not found!")
endif(NOT SDLGFX_FOUND)
if(NOT SDLTTF_FOUND)
   message(FATAL_ERROR "SDL_ttf not found!")
endif(NOT SDLTTF_FOUND)
if(NOT SDLMIXER_FOUND)
   message(FATAL_ERROR "SDL_mixer not found!")
endif(NOT SDLMIXER_FOUND)
if(NOT LIBXML2_FOUND)
   message(FATAL_ERROR "LIBXML2 not found!")
endif(NOT LIBXML2_FOUND)
if(NOT CURL_FOUND)
   message(FATAL_ERROR "CURL not found!")
endif(NOT CURL_FOUND)
include_directories(
   ${PROJECT_SOURCE_DIR}
   ${SDL_INCLUDE_DIR}
   ${SDLIMAGE_INCLUDE_DIR}
   ${SDLGFX_INCLUDE_DIR}
   ${SDLTTF_INCLUDE_DIR}
   ${SDLMIXER_INCLUDE_DIR}
   ${LIBXML2_INCLUDE_DIR}
   ${CURL_INCLUDE_DIR}
) 
link_libraries (
   ${SDL_LIBRARY}
   ${SDLIMAGE_LIBRARY}
   ${SDLGFX_LIBRARY}
   ${SDLTTF_LIBRARY}
   ${SDLMIXER_LIBRARY}
   ${LIBXML2_LIBRARIES}
   ${CURL_LIBRARY}
   ${MY_MATH_LIB}
   pspmaps
)
add_executable(pspmaps ${SOURCES})
