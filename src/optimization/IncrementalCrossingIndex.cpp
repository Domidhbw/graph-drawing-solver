
#include "IncrementalCrossingIndex.hpp"

#include <stdexcept>

namespace
{
    bool haveSameCrossingStats(
        const CrossingStats& first,
        const CrossingStats& second
    )
    {
        return first.totalCrossings == second.totalCrossings
            && first.maxCrossingsPerEdge == second.maxCrossingsPerEdge
            && first.crossingsPerEdge == second.crossingsPerEdge;
    }
}

IncrementalCrossingIndex::IncrementalCrossingIndex(
    const Graph& graph,
    const bool verifyEachMove
)
    : graph_(graph),
      stats_(CrossingStatistics::compute(graph)),
      verifyEachMove_(verifyEachMove)
{
}

const Graph& IncrementalCrossingIndex::graph() const
{
    return graph_;
}

const CrossingStats& IncrementalCrossingIndex::stats() const
{
    return stats_;
}

LayoutScore IncrementalCrossingIndex::score() const
{
    return LayoutScoreCalculator::fromCrossingStats(stats_);
}

bool IncrementalCrossingIndex::isConsistentWithFullRecompute() const
{
    const CrossingStats fullStats = CrossingStatistics::compute(graph_);

    return haveSameCrossingStats(
        stats_,
        fullStats
    );
}

CrossingDeltaResult IncrementalCrossingIndex::evaluateNodeMove(
    const NodeMove& move
) const
{
    return CrossingDelta::evaluateNodeMoveStatsLocally(
        graph_,
        move,
        stats_
    ).scoreDelta;
}

CrossingDeltaResult IncrementalCrossingIndex::evaluateNodeMoveByFullRecompute(
    const NodeMove& move
) const
{
    return CrossingDelta::evaluateNodeMoveByFullRecompute(
        graph_,
        move
    );
}

CrossingDeltaResult IncrementalCrossingIndex::applyNodeMove(
    const NodeMove& move
)
{
    const CrossingDeltaStatsResult deltaStats = CrossingDelta::evaluateNodeMoveStatsLocally(
        graph_,
        move,
        stats_
    );

    graph_.nodes[move.nodeIndex].x = move.newX;
    graph_.nodes[move.nodeIndex].y = move.newY;

    stats_ = deltaStats.afterStats;

    if (verifyEachMove_)
    {
        assertConsistentWithFullRecompute();
    }

    return deltaStats.scoreDelta;
}

void IncrementalCrossingIndex::assertConsistentWithFullRecompute() const
{
    if (!isConsistentWithFullRecompute())
    {
        throw std::runtime_error(
            "IncrementalCrossingIndex is inconsistent with full recompute."
        );
    }
}
