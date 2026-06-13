
#pragma once

#include "Graph.hpp"

#include <cstddef>
#include <vector>

struct CrossingStats
{
    std::size_t totalCrossings = 0;
    std::size_t maxCrossingsPerEdge = 0;
    std::vector<std::size_t> crossingsPerEdge;
};

class CrossingStatistics
{
public:
    static CrossingStats compute(const Graph& graph);
};
