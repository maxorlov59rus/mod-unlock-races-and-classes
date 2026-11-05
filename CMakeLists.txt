# Module registration for AzerothCore build system (mod_-style modules pick this up automatically).

CollectSourceFiles(
  ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE_SOURCES
  "*.cpp"
  "*.h"
)

add_library(mod-unlock-races-classes STATIC ${PRIVATE_SOURCES})

target_link_libraries(mod-unlock-races-classes
  PRIVATE
    game
    scripts
)

set_target_properties(mod-unlock-races-classes PROPERTIES FOLDER "modules")
