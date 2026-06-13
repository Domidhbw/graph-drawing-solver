#pragma once

#include "Graph.hpp"

class FruchtermanReingoldLayout
{
public:
    static void apply(
        Graph& graph,
        double width,
        double height,
        int iterations
    );
};
