
#pragma once

#include "Graph.hpp"
#include "LeafReducer.hpp"

class LeafReinserter
{
public:
    static Graph reinsertLeaves(const LeafReductionResult& reductionResult);
};
