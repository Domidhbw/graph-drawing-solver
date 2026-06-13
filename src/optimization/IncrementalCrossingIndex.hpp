
#pragma once

#include "CrossingDelta.hpp"
#include "CrossingStatistics.hpp"
#include "Graph.hpp"
#include "LayoutScore.hpp"

class IncrementalCrossingIndex
{
public:
    // verifyEachMove pruefte nach jedem angewendeten Zug die inkrementell
    // gepflegte Statistik gegen eine vollstaendige Neuberechnung. Das ist nur
    // fuer Korrektheitstests gedacht und standardmaessig deaktiviert, da es die
    // Laufzeitersparnis sonst wieder aufheben wuerde.
    explicit IncrementalCrossingIndex(
        const Graph& graph,
        bool verifyEachMove = false
    );

    const Graph& graph() const;
    const CrossingStats& stats() const;
    LayoutScore score() const;

    bool isConsistentWithFullRecompute() const;

    CrossingDeltaResult evaluateNodeMove(
        const NodeMove& move
    ) const;

    CrossingDeltaResult evaluateNodeMoveByFullRecompute(
        const NodeMove& move
    ) const;

    CrossingDeltaResult applyNodeMove(
        const NodeMove& move
    );

private:
    Graph graph_;
    CrossingStats stats_;
    bool verifyEachMove_ = false;

    void assertConsistentWithFullRecompute() const;
};
