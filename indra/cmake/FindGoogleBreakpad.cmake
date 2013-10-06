# -*- cmake -*-

# - Find Google BreakPad
# Find the Google BreakPad includes and library
# This module defines
#  BREAKPAD_INCLUDE_DIRECTORIES, where to find the Goole BreakPad includes.
#  BREAKPAD_EXCEPTION_HANDLER_LIBRARIES, the libraries needed to use Google BreakPad.
#  BREAKPAD_EXCEPTION_HANDLER_FOUND, If false, do not try to use Google BreakPad.
# also defined, but not for general use are
#  BREAKPAD_EXCEPTION_HANDLER_LIBRARY, where to find the Google BreakPad library.

FIND_PATH(BREAKPAD_INCLUDE_DIRECTORIES common/using_std_string.h PATH_SUFFIXES google_breakpad)

SET(BREAKPAD_EXCEPTION_HANDLER_NAMES ${BREAKPAD_EXCEPTION_HANDLER_NAMES} breakpad_client)
FIND_LIBRARY(BREAKPAD_EXCEPTION_HANDLER_LIBRARY
  NAMES ${BREAKPAD_EXCEPTION_HANDLER_NAMES}
  )

IF (BREAKPAD_EXCEPTION_HANDLER_LIBRARY AND BREAKPAD_INCLUDE_DIRECTORIES)
    SET(BREAKPAD_EXCEPTION_HANDLER_LIBRARIES ${BREAKPAD_EXCEPTION_HANDLER_LIBRARY})
    SET(BREAKPAD_EXCEPTION_HANDLER_FOUND "YES")
ELSE (BREAKPAD_EXCEPTION_HANDLER_LIBRARY AND BREAKPAD_INCLUDE_DIRECTORIES)
    SET(BREAKPAD_EXCEPTION_HANDLER_FOUND "NO")
ENDIF (BREAKPAD_EXCEPTION_HANDLER_LIBRARY AND BREAKPAD_INCLUDE_DIRECTORIES)


IF (BREAKPAD_EXCEPTION_HANDLER_FOUND)
   IF (NOT BREAKPAD_EXCEPTION_HANDLER_FIND_QUIETLY)
      MESSAGE(STATUS "Found Google BreakPad: ${BREAKPAD_EXCEPTION_HANDLER_LIBRARIES}")
   ENDIF (NOT BREAKPAD_EXCEPTION_HANDLER_FIND_QUIETLY)
ELSE (BREAKPAD_EXCEPTION_HANDLER_FOUND)
   IF (BREAKPAD_EXCEPTION_HANDLER_FIND_REQUIRED)
      MESSAGE(FATAL_ERROR "Could not find Google BreakPad library")
   ENDIF (BREAKPAD_EXCEPTION_HANDLER_FIND_REQUIRED)
ENDIF (BREAKPAD_EXCEPTION_HANDLER_FOUND)

MARK_AS_ADVANCED(
  BREAKPAD_EXCEPTION_HANDLER_LIBRARY
  BREAKPAD_INCLUDE_DIRECTORIES
  )
