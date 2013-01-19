# - Locate SDL_gfx library
# This module defines
#  SDLGFX_LIBRARY, the library to link against
#  SDLGFX_FOUND, if false, do not try to link to SDL
#  SDLGFX_INCLUDE_DIR, where to find SDL/SDL_gfxPrimitives.h
#
# $SDLDIR is an environment variable that would
# correspond to the ./configure --prefix=$SDLDIR
# used in building SDL.
#
# Created by Olivier DOLE. This was copied from the FindSDL_image.cmake 
# module.

# On OSX, this will prefer the Framework version (if found) over others.
# People will have to manually change the cache values of 
# SDLGFX_LIBRARY to override this selection.
FIND_PATH(SDLGFX_INCLUDE_DIR SDL_gfxPrimitives.h
  $ENV{SDLGFXDIR}/include
  $ENV{SDLDIR}/include
  ~/Library/Frameworks/SDL_gfx.framework/Headers
  /Library/Frameworks/SDL_gfx.framework/Headers
  /usr/local/include/SDL
  /usr/include/SDL
  /usr/local/include/SDL12
  /usr/local/include/SDL11 # FreeBSD ports
  /usr/include/SDL12
  /usr/include/SDL11
  /usr/local/include
  /usr/include
  /sw/include/SDL # Fink
  /sw/include
  /opt/local/include/SDL # DarwinPorts
  /opt/local/include
  /opt/csw/include/SDL # Blastwave
  /opt/csw/include 
  /opt/include/SDL
  /opt/include
  )
# I'm not sure if I should do a special casing for Apple. It is 
# unlikely that other Unix systems will find the framework path.
# But if they do ([Next|Open|GNU]Step?), 
# do they want the -framework option also?
IF(${SDLGFX_INCLUDE_DIR} MATCHES ".framework")
  # Extract the path the framework resides in so we can use it for the -F flag
  STRING(REGEX REPLACE "(.*)/.*\\.framework/.*" "\\1" SDLGFX_FRAMEWORK_PATH_TEMP ${SDLGFX_INCLUDE_DIR})
  IF("${SDLGFX_FRAMEWORK_PATH_TEMP}" STREQUAL "/Library/Frameworks"
      OR "${SDLGFX_FRAMEWORK_PATH_TEMP}" STREQUAL "/System/Library/Frameworks"
      )
    # String is in default search path, don't need to use -F
    SET(SDLGFX_LIBRARY "-framework SDL_gfx" CACHE STRING "SDL_gfx framework for OSX")
  ELSE("${SDLGFX_FRAMEWORK_PATH_TEMP}" STREQUAL "/Library/Frameworks"
      OR "${SDLGFX_FRAMEWORK_PATH_TEMP}" STREQUAL "/System/Library/Frameworks"
      )
    # String is not /Library/Frameworks, need to use -F
    SET(SDLGFX_LIBRARY "-F${SDLGFX_FRAMEWORK_PATH_TEMP} -framework SDL_gfx" CACHE STRING "SDL_gfx framework for OSX")
  ENDIF("${SDLGFX_FRAMEWORK_PATH_TEMP}" STREQUAL "/Library/Frameworks"
    OR "${SDLGFX_FRAMEWORK_PATH_TEMP}" STREQUAL "/System/Library/Frameworks"
    )
  # Clear the temp variable so nobody can see it
  SET(SDLGFX_FRAMEWORK_PATH_TEMP "" CACHE INTERNAL "")

ELSE(${SDLGFX_INCLUDE_DIR} MATCHES ".framework")
  FIND_LIBRARY(SDLGFX_LIBRARY 
    NAMES SDL_gfx
    PATHS
    $ENV{SDLGFXDIR}/lib
    $ENV{SDLDIR}/lib
    /usr/local/lib
    /usr/lib
    /sw/lib
    /opt/local/lib
    /opt/csw/lib
    /opt/lib
    )
ENDIF(${SDLGFX_INCLUDE_DIR} MATCHES ".framework")

if(SDLGFX_INCLUDE_DIR AND EXISTS "${SDLGFX_INCLUDE_DIR}/SDL_gfxPrimitives.h")
  file(STRINGS "${SDLGFX_INCLUDE_DIR}/SDL_gfxPrimitives.h" SDL_GFXPRIMITIVES_MAJOR_LINE REGEX "^#define[ \t]+SDL_GFXPRIMITIVES_MAJOR[ \t]+[0-9]+$")
  file(STRINGS "${SDLGFX_INCLUDE_DIR}/SDL_gfxPrimitives.h" SDL_GFXPRIMITIVES_MINOR_LINE REGEX "^#define[ \t]+SDL_GFXPRIMITIVES_MINOR[ \t]+[0-9]+$")
  file(STRINGS "${SDLGFX_INCLUDE_DIR}/SDL_gfxPrimitives.h" SDL_GFXPRIMITIVES_MICRO_LINE REGEX "^#define[ \t]+SDL_GFXPRIMITIVES_MICRO[ \t]+[0-9]+$")
  string(REGEX REPLACE "^#define[ \t]+SDL_GFXPRIMITIVES_MAJOR[ \t]+([0-9]+)$" "\\1" SDL_GFXPRIMITIVES_MAJOR "${SDL_GFXPRIMITIVES_MAJOR_LINE}")
  string(REGEX REPLACE "^#define[ \t]+SDL_GFXPRIMITIVES_MINOR[ \t]+([0-9]+)$" "\\1" SDL_GFXPRIMITIVES_MINOR "${SDL_GFXPRIMITIVES_MINOR_LINE}")
  string(REGEX REPLACE "^#define[ \t]+SDL_GFXPRIMITIVES_MICRO[ \t]+([0-9]+)$" "\\1" SDL_GFXPRIMITIVES_MICRO "${SDL_GFXPRIMITIVES_MICRO_LINE}")
  set(SDL_GFXPRIMITIVES_VERSION_STRING ${SDL_GFXPRIMITIVES_MAJOR}.${SDL_GFXPRIMITIVES_MINOR}.${SDL_GFXPRIMITIVES_MICRO})
  unset(SDL_GFXPRIMITIVES_MAJOR_LINE)
  unset(SDL_GFXPRIMITIVES_MINOR_LINE)
  unset(SDL_GFXPRIMITIVES_MICRO_LINE)
  unset(SDL_GFXPRIMITIVES_MAJOR)
  unset(SDL_GFXPRIMITIVES_MINOR)
  unset(SDL_GFXPRIMITIVES_MICRO)
endif()

SET(SDLGFX_FOUND FALSE)
IF(SDLGFX_LIBRARY AND SDLGFX_INCLUDE_DIR)
  MESSAGE(STATUS "Found SDL_gfx: ${SDLGFX_LIBRARY} (found version \"${SDL_GFXPRIMITIVES_VERSION_STRING}\")")
  SET(SDLGFX_FOUND TRUE)
ENDIF(SDLGFX_LIBRARY AND SDLGFX_INCLUDE_DIR)

