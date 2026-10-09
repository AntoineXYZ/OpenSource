au_add_static_library(AuDesigner "${CMAKE_CURRENT_SOURCE_DIR}" DEFINE AUDESIGNER)
get_target_property(au_designer_sources AuDesigner SOURCES)
list(FILTER au_designer_sources EXCLUDE REGEX "/legacy/aulegacyship(mesh|equipment|stairs|vehicles)\\.cpp$")
set_property(TARGET AuDesigner PROPERTY SOURCES "${au_designer_sources}")
target_link_libraries(AuDesigner PUBLIC
    AuUtil AuMath AuGlobal AuGL AuData AuAtom AuLoginClient AuMail AuClient
    SDL2::SDL2
)
target_include_directories(AuDesigner PRIVATE
    "${CMAKE_SOURCE_DIR}/src/app/servers/scene"
)
