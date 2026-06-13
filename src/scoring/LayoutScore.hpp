#pragma once

#include "CrossingStatistics.hpp"
#include "Graph.hpp"

#include <cstddef>

struct LayoutScore
{
    std::size_t maxCrossingsPerEdge = 0;
    std::size_t totalCrossings = 0;
};

class LayoutScoreCalculator
{
public:
    static LayoutScore fromCrossingStats(const CrossingStats& stats);
    static LayoutScore compute(const Graph& graph);

    static bool isBetter(
        const LayoutScore& candidate,
        const LayoutScore& currentBest
    );
};
