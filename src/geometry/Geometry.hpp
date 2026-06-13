#pragma once

#include "Graph.hpp"

namespace Geometry
{
    double orientation(const Node& a, const Node& b, const Node& c);

    bool segmentsIntersect(
        const Node& a,
        const Node& b,
        const Node& c,
        const Node& d
    );
}
