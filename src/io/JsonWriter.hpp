
#pragma once

#include "Graph.hpp"

#include <string>

class JsonWriter
{
public:
    static void save(const Graph& graph, const std::string& path);
    static void saveSubmission(const Graph& graph, const std::string& path);
};
