cmake_minimum_required(VERSION 3.16)
project(GFEMesh VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default OFF: link an external prebuilt Gmsh via GMSH_ROOT.
# Corresponding-source / self-contained builds: -DGFEMesh_BUILD_GMSH=ON.
option(GFEMesh_BUILD_GMSH "Build vendored third_party/gmsh and link mesh_gen" OFF)
option(GFEMesh_BUILD_TESTS "Build tests" OFF)

if(NOT GFEMesh_OCCT_ROOT)
  message(FATAL_ERROR "Set -DGFEMesh_OCCT_ROOT=... (OCCT root with inc/ and win64/vc14/lib)")
endif()
if(NOT GFEMesh_BUILD_GMSH AND NOT GMSH_ROOT)
  message(FATAL_ERROR "Set -DGMSH_ROOT=... or enable -DGFEMesh_BUILD_GMSH=ON")
endif()

# --- Gmsh ---
set(GFEMesh_GMSH_TARGET "")
if(GFEMesh_BUILD_GMSH)
  # Shared DLL, public API only. DYNAMIC=ON → MSVC LNK1189; OpenMP off on MSVC.
  set(ENABLE_BUILD_GUI OFF CACHE BOOL "" FORCE)
  set(ENABLE_BUILD_SHARED ON CACHE BOOL "" FORCE)
  set(ENABLE_BUILD_LIB OFF CACHE BOOL "" FORCE)
  set(ENABLE_BUILD_DYNAMIC OFF CACHE BOOL "" FORCE)
  set(ENABLE_PRIVATE_API OFF CACHE BOOL "" FORCE)
  set(ENABLE_OPENMP OFF CACHE BOOL "" FORCE)
  add_subdirectory(third_party/gmsh)
  set(GFEMesh_GMSH_TARGET shared)
elseif(GMSH_ROOT)
  add_library(GFEMesh_gmsh_imported SHARED IMPORTED GLOBAL)
  set(_gmsh_implib_r "${GMSH_ROOT}/Release/gmsh.lib")
  set(_gmsh_dll_r "${GMSH_ROOT}/Release/gmsh.dll")
  set(_gmsh_implib_d "${GMSH_ROOT}/Debug/gmsh.lib")
  set(_gmsh_dll_d "${GMSH_ROOT}/Debug/gmsh.dll")
  if(NOT EXISTS "${_gmsh_implib_d}")
    set(_gmsh_implib_d "${_gmsh_implib_r}")
    set(_gmsh_dll_d "${_gmsh_dll_r}")
  endif()
  set_target_properties(GFEMesh_gmsh_imported PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${GMSH_ROOT}/include"
    IMPORTED_CONFIGURATIONS "Release;RelWithDebInfo;MinSizeRel;Debug"
    IMPORTED_IMPLIB_RELEASE "${_gmsh_implib_r}"
    IMPORTED_LOCATION_RELEASE "${_gmsh_dll_r}"
    IMPORTED_IMPLIB_RELWITHDEBINFO "${_gmsh_implib_r}"
    IMPORTED_LOCATION_RELWITHDEBINFO "${_gmsh_dll_r}"
    IMPORTED_IMPLIB_MINSIZEREL "${_gmsh_implib_r}"
    IMPORTED_LOCATION_MINSIZEREL "${_gmsh_dll_r}"
    IMPORTED_IMPLIB_DEBUG "${_gmsh_implib_d}"
    IMPORTED_LOCATION_DEBUG "${_gmsh_dll_d}"
  )
  set(GFEMesh_GMSH_TARGET GFEMesh_gmsh_imported)
else()
  message(FATAL_ERROR "Set GMSH_ROOT or enable GFEMesh_BUILD_GMSH=ON")
endif()

# --- OCCT (inc + win64/vc14/lib|libd) ---
if(NOT EXISTS "${GFEMesh_OCCT_ROOT}/inc")
  message(FATAL_ERROR "OCCT includes not found under GFEMesh_OCCT_ROOT=${GFEMesh_OCCT_ROOT}")
endif()
add_library(GFEMesh_occt INTERFACE)
target_include_directories(GFEMesh_occt INTERFACE "${GFEMesh_OCCT_ROOT}/inc")
file(GLOB _occ_r "${GFEMesh_OCCT_ROOT}/win64/vc14/lib/*.lib")
file(GLOB _occ_d "${GFEMesh_OCCT_ROOT}/win64/vc14/libd/*.lib")
if(NOT _occ_r)
  message(FATAL_ERROR "No OCCT Release libs under ${GFEMesh_OCCT_ROOT}/win64/vc14/lib")
endif()
set(_occ_link)
foreach(_l IN LISTS _occ_d)
  list(APPEND _occ_link "$<$<CONFIG:Debug>:${_l}>")
endforeach()
foreach(_l IN LISTS _occ_r)
  list(APPEND _occ_link "$<$<NOT:$<CONFIG:Debug>>:${_l}>")
endforeach()
if(NOT _occ_d)
  foreach(_l IN LISTS _occ_r)
    list(APPEND _occ_link "$<$<CONFIG:Debug>:${_l}>")
  endforeach()
endif()
target_link_libraries(GFEMesh_occt INTERFACE ${_occ_link})

# --- mesh_gen.exe ---
add_executable(mesh_gen
  src/main.cpp
  src/JobFile.cpp
  src/GeomIO.cpp
  src/MeshCore.cpp
  src/MeshTxtIO.cpp
  src/Protocol.h
  src/Log.h
  src/JobFile.h
  src/GeomIO.h
  src/MeshCore.h
  src/MeshTxtIO.h
)

target_compile_definitions(mesh_gen PRIVATE GFEMesh_VERSION="${PROJECT_VERSION}")
if(MSVC)
  target_compile_options(mesh_gen PRIVATE /utf-8 /EHsc)
  target_compile_definitions(mesh_gen PRIVATE _USE_MATH_DEFINES)
endif()

target_link_libraries(mesh_gen PRIVATE ${GFEMesh_GMSH_TARGET} GFEMesh_occt)

set_target_properties(mesh_gen PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  DEBUG_POSTFIX "_d"
)

# Copy gmsh.dll next to mesh_gen (vendored shared or prebuilt import).
if(TARGET shared)
  add_custom_command(TARGET mesh_gen POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:shared>"
      "$<TARGET_FILE_DIR:mesh_gen>"
    COMMENT "Copy gmsh.dll beside mesh_gen"
  )
elseif(TARGET GFEMesh_gmsh_imported)
  add_custom_command(TARGET mesh_gen POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "$<TARGET_FILE:GFEMesh_gmsh_imported>"
      "$<TARGET_FILE_DIR:mesh_gen>"
    COMMENT "Copy gmsh.dll beside mesh_gen"
  )
endif()

install(TARGETS mesh_gen RUNTIME DESTINATION bin)
install(FILES
  LICENSE NOTICE THIRD_PARTY.md README.md BUILD.md
  DESTINATION share/GFEMesh
)
install(DIRECTORY licenses DESTINATION share/GFEMesh)
install(DIRECTORY docs DESTINATION share/GFEMesh)
install(DIRECTORY examples DESTINATION share/GFEMesh)

if(GFEMesh_BUILD_TESTS)
  enable_testing()
endif()

# Optional smoke-test helper (not installed)
add_executable(MakeBox EXCLUDE_FROM_ALL examples/tools/make_box.cpp)
target_link_libraries(MakeBox PRIVATE GFEMesh_occt)
if(MSVC)
  target_compile_options(MakeBox PRIVATE /utf-8 /EHsc)
endif()
set_target_properties(MakeBox PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
