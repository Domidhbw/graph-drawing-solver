
#include "CrossingCounter.hpp"

#include "CrossingStatistics.hpp"

std::size_t CrossingCounter::countCrossings(const Graph& graph)
{
    return CrossingStatistics::compute(graph).totalCrossings;
}
