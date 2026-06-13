
#pragma once

#include "Graph.hpp"

#include <vector>

struct RemovedLeaf
{
    Node node;
    Edge edge;
    int parentNodeId = 0;
};

struct LeafReductionResult
{
    Graph reducedGraph;
    std::vector<RemovedLeaf> removedLeaves;

    bool changed() const
    {
        return !removedLeaves.empty();
    }
};

class LeafReducer
{
public:
    static LeafReductionResult reduceSinglePass(const Graph& graph);
};
