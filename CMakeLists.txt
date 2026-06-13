cmake_minimum_required(VERSION 3.20)

project(graph_drawing_contest_2026 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(PROJECT_INCLUDE_DIRS
    src/domain
    src/geometry
    src/scoring
    src/io
    src/validation
    src/layout
    src/optimization
    src/preprocessing
    src/checkpoint
)

set(COMMON_SOURCES
    src/io/JsonReader.cpp
    src/io/JsonWriter.cpp
    src/geometry/Geometry.cpp
    src/geometry/CrossingCounter.cpp
    src/geometry/CrossingStatistics.cpp
    src/optimization/CrossingDelta.cpp
    src/optimization/IncrementalCrossingIndex.cpp
    src/scoring/LayoutScore.cpp
    src/validation/SolutionValidator.cpp
    src/validation/SolutionRepairer.cpp
    src/layout/BaselineLayouts.cpp
    src/layout/FruchtermanReingoldLayout.cpp
    src/optimization/SimulatedAnnealingOptimizer.cpp
    src/preprocessing/LeafReducer.cpp
    src/preprocessing/LeafReinserter.cpp
    src/checkpoint/CheckpointStore.cpp
    src/checkpoint/BestSolutionStore.cpp
)

add_executable(solver
    src/apps/main.cpp
    ${COMMON_SOURCES}
)

target_include_directories(solver PRIVATE
    ${PROJECT_INCLUDE_DIRS}
)

find_package(nlohmann_json REQUIRED)

target_link_libraries(solver PRIVATE
    nlohmann_json::nlohmann_json
)

add_executable(crossing_delta_probe
    src/apps/crossing_delta_probe.cpp
    src/optimization/CrossingDelta.cpp
    src/io/JsonReader.cpp
    src/geometry/Geometry.cpp
    src/geometry/CrossingStatistics.cpp
    src/scoring/LayoutScore.cpp
)

target_include_directories(crossing_delta_probe PRIVATE
    ${PROJECT_INCLUDE_DIRS}
)

target_link_libraries(crossing_delta_probe PRIVATE
    nlohmann_json::nlohmann_json
)

add_executable(incremental_crossing_index_probe
    src/apps/incremental_crossing_index_probe.cpp
    src/optimization/IncrementalCrossingIndex.cpp
    src/optimization/CrossingDelta.cpp
    src/io/JsonReader.cpp
    src/geometry/Geometry.cpp
    src/geometry/CrossingStatistics.cpp
    src/scoring/LayoutScore.cpp
)

target_include_directories(incremental_crossing_index_probe PRIVATE
    ${PROJECT_INCLUDE_DIRS}
)

target_link_libraries(incremental_crossing_index_probe PRIVATE
    nlohmann_json::nlohmann_json
)

add_executable(leaf_reducer_probe
    src/apps/leaf_reducer_probe.cpp
    src/preprocessing/LeafReducer.cpp
    src/io/JsonReader.cpp
)

target_include_directories(leaf_reducer_probe PRIVATE
    ${PROJECT_INCLUDE_DIRS}
)

target_link_libraries(leaf_reducer_probe PRIVATE
    nlohmann_json::nlohmann_json
)

add_executable(leaf_reinsertion_probe
    src/apps/leaf_reinsertion_probe.cpp
    src/preprocessing/LeafReducer.cpp
    src/preprocessing/LeafReinserter.cpp
    src/io/JsonReader.cpp
    src/validation/SolutionValidator.cpp
)

target_include_directories(leaf_reinsertion_probe PRIVATE
    ${PROJECT_INCLUDE_DIRS}
)

target_link_libraries(leaf_reinsertion_probe PRIVATE
    nlohmann_json::nlohmann_json
)

add_executable(checkpoint_probe
    src/apps/checkpoint_probe.cpp
    src/checkpoint/CheckpointStore.cpp
    src/io/JsonReader.cpp
    src/io/JsonWriter.cpp
    src/validation/SolutionValidator.cpp
)

target_include_directories(checkpoint_probe PRIVATE
    ${PROJECT_INCLUDE_DIRS}
)

target_link_libraries(checkpoint_probe PRIVATE
    nlohmann_json::nlohmann_json
)

add_executable(best_solution_store_probe
    src/apps/best_solution_store_probe.cpp
    src/checkpoint/BestSolutionStore.cpp
    src/checkpoint/CheckpointStore.cpp
    src/io/JsonReader.cpp
    src/io/JsonWriter.cpp
    src/geometry/Geometry.cpp
    src/geometry/CrossingStatistics.cpp
    src/scoring/LayoutScore.cpp
    src/validation/SolutionValidator.cpp
)

target_include_directories(best_solution_store_probe PRIVATE
    ${PROJECT_INCLUDE_DIRS}
)

target_link_libraries(best_solution_store_probe PRIVATE
    nlohmann_json::nlohmann_json
)

add_executable(checkpointed_sa_probe
    src/apps/checkpointed_sa_probe.cpp
    ${COMMON_SOURCES}
)

target_include_directories(checkpointed_sa_probe PRIVATE
    ${PROJECT_INCLUDE_DIRS}
)

target_link_libraries(checkpointed_sa_probe PRIVATE
    nlohmann_json::nlohmann_json
)

add_executable(contest_runner_probe
    src/apps/contest_runner_probe.cpp
)

add_dependencies(contest_runner_probe
    solver
    checkpointed_sa_probe
)

add_executable(contest_runner_timeboxed_probe
    src/apps/contest_runner_timeboxed_probe.cpp
)

add_dependencies(contest_runner_timeboxed_probe
    solver
    checkpointed_sa_probe
)

add_executable(contest_runner_adaptive_probe
    src/apps/contest_runner_adaptive_probe.cpp
)

add_dependencies(contest_runner_adaptive_probe
    solver
    checkpointed_sa_probe
)


find_package(ogdf CONFIG QUIET)

if(TARGET OGDF)
    add_executable(ogdf_probe
        src/apps/ogdf_probe.cpp
    )

    target_link_libraries(ogdf_probe PRIVATE
        OGDF
        COIN
    )

    add_executable(ogdf_layout
        src/apps/ogdf_layout.cpp
        src/io/JsonReader.cpp
        src/io/JsonWriter.cpp
        src/validation/SolutionValidator.cpp
        src/validation/SolutionRepairer.cpp
    )

    target_include_directories(ogdf_layout PRIVATE
        ${PROJECT_INCLUDE_DIRS}
    )

    target_link_libraries(ogdf_layout PRIVATE
        nlohmann_json::nlohmann_json
        OGDF
        COIN
    )
    

    message(STATUS "OGDF found: building ogdf_probe and ogdf_layout")
else()
    message(STATUS "OGDF not found: skipping OGDF tools")
endif()
