
#pragma once

#include "Graph.hpp"

#include <cstddef>

class CrossingCounter
{
public:
    static std::size_t countCrossings(const Graph& graph);
};
