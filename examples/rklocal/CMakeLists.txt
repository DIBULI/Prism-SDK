# Compatibility entry point; documentation and standalone build live together.
cmake_minimum_required(VERSION 3.20)
project(PrismRkLocalExamples VERSION 1.1.0 LANGUAGES CXX)
include(CTest)
add_subdirectory("../../rk-local-sdk" "rk-local-sdk")
