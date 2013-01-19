cmake_minimum_required(VERSION 2.6)
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
project(pspmaps)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR})
set(CMAKE_CURRENT_SOURCE_DIR ${PROJECT_SOURCE_DIR})
set(
   SOURCES
   global.c
   kml.c
   pspmaps.c
   tile.c
   io.c
)
Find_Package(SDL REQUIRED)
Find_Package(SDL_image REQUIRED)
# Find_Package(SDL_gfx REQUIRED)
Find_Package(SDL_ttf REQUIRED)
Find_Package(SDL_mixer REQUIRED)
Find_Package(LibXml2 REQUIRED)
Find_Package(CURL REQUIRED)
if(NOT SDL_FOUND)
   message(FATAL_ERROR "SDL not found!")
endif(NOT SDL_FOUND)
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
   ${SDLTTF_INCLUDE_DIR}
   ${SDLMIXER_INCLUDE_DIR}
   ${LIBXML2_INCLUDE_DIR}
   ${CURL_INCLUDE_DIR}
) 
link_libraries (
   ${SDL_LIBRARY}
   ${SDLIMAGE_LIBRARY}
   ${SDLTTF_LIBRARY}
   ${SDLMIXER_LIBRARY}
   ${LIBXML2_LIBRARIES}
   ${CURL_LIBRARY}
   pspmaps
)
add_executable(pspmaps ${SOURCES})
