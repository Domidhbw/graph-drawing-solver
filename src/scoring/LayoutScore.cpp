#include "LayoutScore.hpp"

LayoutScore LayoutScoreCalculator::fromCrossingStats(const CrossingStats& stats)
{
    return LayoutScore{
        stats.maxCrossingsPerEdge,
        stats.totalCrossings
    };
}

LayoutScore LayoutScoreCalculator::compute(const Graph& graph)
{
    return fromCrossingStats(CrossingStatistics::compute(graph));
}

bool LayoutScoreCalculator::isBetter(
    const LayoutScore& candidate,
    const LayoutScore& currentBest
)
{
    if (candidate.maxCrossingsPerEdge != currentBest.maxCrossingsPerEdge)
    {
        return candidate.maxCrossingsPerEdge < currentBest.maxCrossingsPerEdge;
    }

    return candidate.totalCrossings < currentBest.totalCrossings;
}
