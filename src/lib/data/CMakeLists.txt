au_add_static_library(AuData "${CMAKE_CURRENT_SOURCE_DIR}" DEFINE AUDATA)
target_link_libraries(AuData PUBLIC AuGlobal AuUtil AuMath AuGL)

# Procedural legacy geometry is shared by the offline designer and server intake.
# Keep its source files in place while compiling them in the data library.
set(AU_LEGACY_GEOMETRY_DIR "${CMAKE_SOURCE_DIR}/src/lib/designer/legacy")
target_sources(AuData PRIVATE
    "${AU_LEGACY_GEOMETRY_DIR}/aulegacyshipmesh.cpp"
    "${AU_LEGACY_GEOMETRY_DIR}/aulegacyshipequipment.cpp"
    "${AU_LEGACY_GEOMETRY_DIR}/aulegacyshipstairs.cpp"
    "${AU_LEGACY_GEOMETRY_DIR}/aulegacyshipvehicles.cpp"
)
target_include_directories(AuData PUBLIC "${AU_LEGACY_GEOMETRY_DIR}")
