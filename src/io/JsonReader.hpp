
#pragma once

#include "Graph.hpp"

#include <string>

class JsonReader
{
public:
    static Graph load(const std::string& path);
};
