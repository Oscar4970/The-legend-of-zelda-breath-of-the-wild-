cmake_minimum_required(VERSION 3.22.1)
project(botwgpu LANGUAGES CXX)
add_library(botwgpu SHARED gpu_info.cpp nso_inspector.cpp nso_memory_jni.cpp synthetic_exec.cpp linked_exec.cpp mod0_jni.cpp nro_inspector_jni.cpp relr_simulator_jni.cpp nro_runtime_audit_jni.cpp vulkan_runtime_probe.cpp vulkan_offscreen_test.cpp vulkan_triangle_test.cpp switch_container_inspector_jni.cpp switch_deep_analyzer_jni.cpp switch_nca_bridge_jni.cpp switch_nsp_vfs_jni.cpp)
# AArch64 source -> upstream static recompilation -> generated C++ is run in Android.
# Generated files come from the pinned public MIT recompiler and our own NSO.
target_sources(botwgpu PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../recomp_aot/aot_android_jni.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/aot_generated/output/code_000.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../recomp_aot/homebrew_test_runtime.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../recomp_aot/homebrew_test_jni.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/aot_generated/homebrew/code_000.cpp)
target_include_directories(botwgpu PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/aot_generated/include
  ${CMAKE_CURRENT_SOURCE_DIR}/aot_generated/output
  ${CMAKE_CURRENT_SOURCE_DIR}/aot_generated)
target_compile_features(botwgpu PRIVATE cxx_std_20)
target_link_libraries(botwgpu vulkan)
