#include "BestSolutionStore.hpp"

#include "CheckpointStore.hpp"
#include "SolutionValidator.hpp"

#include <stdexcept>
#include <utility>

namespace
{
    bool isValidSolution(const Graph& graph)
    {
        return SolutionValidator::validate(graph).isValid();
    }

    LayoutScore computeScore(const Graph& graph)
    {
        return LayoutScoreCalculator::compute(graph);
    }
}

BestSolutionStore::BestSolutionStore(std::string outputPath)
    : outputPath_(std::move(outputPath))
{
}

bool BestSolutionStore::hasBestSolution() const
{
    return hasBestSolution_;
}

const LayoutScore& BestSolutionStore::bestScore() const
{
    if (!hasBestSolution_)
    {
        throw std::runtime_error("No best solution has been stored yet.");
    }

    return bestScore_;
}

bool BestSolutionStore::consider(const Graph& candidateGraph)
{
    if (!isValidSolution(candidateGraph))
    {
        return false;
    }

    const LayoutScore candidateScore = computeScore(candidateGraph);

    if (hasBestSolution_ && !LayoutScoreCalculator::isBetter(candidateScore, bestScore_))
    {
        return false;
    }

    CheckpointStore::saveValidAtomic(
        candidateGraph,
        outputPath_
    );

    bestScore_ = candidateScore;
    hasBestSolution_ = true;

    return true;
}
