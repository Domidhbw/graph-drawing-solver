#pragma once
#include "Graph.hpp"
#include "LayoutScore.hpp"
#include "CrossingStatistics.hpp"
#include <cstddef>

struct NodeMove
{
    std::size_t nodeIndex = 0;
    double newX = 0.0;
    double newY = 0.0;
};

struct CrossingDeltaResult
{
    LayoutScore beforeScore;
    LayoutScore afterScore;
    long long deltaMaxCrossingsPerEdge = 0;
    long long deltaTotalCrossings = 0;
};

struct CrossingDeltaStatsResult
{
    CrossingStats beforeStats;
    CrossingStats afterStats;
    CrossingDeltaResult scoreDelta;
};

class CrossingDelta
{
public:
    static CrossingDeltaResult evaluateNodeMoveByFullRecompute(
        const Graph& graph,
        const NodeMove& move
    );

    static CrossingDeltaResult evaluateNodeMoveLocally(
        const Graph& graph,
        const NodeMove& move
    );

    static CrossingDeltaStatsResult evaluateNodeMoveStatsLocally(
        const Graph& graph,
        const NodeMove& move
    );

    // Wie oben, nutzt aber die bereits bekannten Kreuzungsstatistiken als
    // Ausgangspunkt, statt sie vollstaendig neu zu berechnen. Dadurch kostet
    // die Bewertung nur die Pruefung der zum bewegten Knoten inzidenten Kanten.
    static CrossingDeltaStatsResult evaluateNodeMoveStatsLocally(
        const Graph& graph,
        const NodeMove& move,
        const CrossingStats& beforeStats
    );
};


