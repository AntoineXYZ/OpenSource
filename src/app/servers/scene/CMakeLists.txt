au_add_executable(AuSceneServer "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(AuSceneServer PRIVATE
    AuAtom AuData AuGlobal AuUtil AuGL AuMath AuLoginClient AuMail
    AuAtomRes AuModelRes AuBodyPartsRes AuStoryRes AuShipTex0
)
