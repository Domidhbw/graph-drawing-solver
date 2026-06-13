
#pragma once

#include "Graph.hpp"
#include "LayoutScore.hpp"

#include <string>

class BestSolutionStore
{
public:
    explicit BestSolutionStore(std::string outputPath);

    bool hasBestSolution() const;
    const LayoutScore& bestScore() const;

    bool consider(const Graph& candidateGraph);

private:
    std::string outputPath_;
    LayoutScore bestScore_;
    bool hasBestSolution_ = false;
};
