
#pragma once

#include <vector>

struct Node
{
    int id;
    double x;
    double y;
    bool fixed = false;
};

struct Edge
{
    int source;
    int target;
};

struct Graph
{
    std::vector<Node> nodes;
    std::vector<Edge> edges;
    int width = 1'000'000;
    int height = 1'000'000;
};
