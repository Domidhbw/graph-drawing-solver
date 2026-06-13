
#pragma once

#include "Graph.hpp"

#include <string>

class CheckpointStore
{
public:
    static void saveValidAtomic(
        const Graph& graph,
        const std::string& outputPath
    );
};
