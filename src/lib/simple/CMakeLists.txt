# Simple Mode: the design studio's grid building editor, and the generator that builds its plans into building designs.
# The only part of the game written in C++17; the game reaches it through hooks on AuBdDesign (SetSimpleModeGenerator,
# SetSimpleModeDockFactory), installed by the scene client and scene server (AuSimple::RegisterGenerator, RegisterEditor).
au_add_static_library(AuSimple "${CMAKE_CURRENT_SOURCE_DIR}" DEFINE AUSIMPLE)
target_compile_features(AuSimple PRIVATE cxx_std_17)
target_link_libraries(AuSimple PUBLIC AuData AuGlobal AuGL AuMath AuUtil)
