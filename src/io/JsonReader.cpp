
#include "JsonReader.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>
#include <string>

namespace
{
    Node parseNode(const nlohmann::json& jsonNode)
    {
        return Node{
            jsonNode.at("id").get<int>(),
            jsonNode.at("x").get<double>(),
            jsonNode.at("y").get<double>(),
            jsonNode.value("fixed", false)
        };
    }

    Edge parseEdge(const nlohmann::json& jsonEdge)
    {
        return Edge{
            jsonEdge.at("source").get<int>(),
            jsonEdge.at("target").get<int>()
        };
    }

    int readOptionalNonNegativeInt(
        const nlohmann::json& json,
        const std::string& fieldName,
        const int defaultValue
    )
    {
        if (!json.contains(fieldName))
        {
            return defaultValue;
        }

        const int value = json.at(fieldName).get<int>();
        if (value < 0)
        {
            throw std::runtime_error("JSON field must not be negative: " + fieldName);
        }

        return value;
    }
}

Graph JsonReader::load(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open JSON file: " + path);
    }

    nlohmann::json json;
    file >> json;

    Graph graph;
    graph.width = readOptionalNonNegativeInt(json, "width", graph.width);
    graph.height = readOptionalNonNegativeInt(json, "height", graph.height);

    const auto& jsonNodes = json.at("nodes");
    const auto& jsonEdges = json.at("edges");

    graph.nodes.reserve(jsonNodes.size());
    graph.edges.reserve(jsonEdges.size());

    for (const auto& jsonNode : jsonNodes)
    {
        graph.nodes.push_back(parseNode(jsonNode));
    }

    for (const auto& jsonEdge : jsonEdges)
    {
        graph.edges.push_back(parseEdge(jsonEdge));
    }

    return graph;
}
