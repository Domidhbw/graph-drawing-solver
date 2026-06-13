
#pragma once

#include "Graph.hpp"

class BaselineLayouts
{
public:
    static void applyRandom(Graph& graph, double width, double height, unsigned int seed);
    static void applyCircular(Graph& graph, double width, double height);
    static void applyGrid(Graph& graph, double width, double height);
};
