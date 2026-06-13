
#pragma once

#include "Graph.hpp"

#include <string>
#include <vector>

struct ValidationResult
{
    std::vector<std::string> errors;

    bool isValid() const
    {
        return errors.empty();
    }
};

class SolutionValidator
{
public:
    static ValidationResult validate(const Graph& graph);
};
