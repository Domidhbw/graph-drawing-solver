
#include "JsonWriter.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <stdexcept>

namespace
{
    int roundAndClampCoordinate(const double value, const int maxValue)
    {
        const long long rounded = std::llround(value);
        const long long clamped = std::max(
            0LL,
            std::min(static_cast<long long>(maxValue), rounded)
        );

        return static_cast<int>(clamped);
    }

    nlohmann::json serializeNode(
        const Node& node,
        const Graph& graph,
        const bool includeInternalMetadata
    )
    {
        nlohmann::json jsonNode = {
            { "id", node.id },
            { "x", roundAndClampCoordinate(node.x, graph.width) },
            { "y", roundAndClampCoordinate(node.y, graph.height) }
        };

        if (includeInternalMetadata && node.fixed)
        {
            jsonNode["fixed"] = true;
        }

        return jsonNode;
    }

    void saveGraph(
        const Graph& graph,
        const std::string& path,
        const bool includeInternalMetadata
    )
    {
        nlohmann::json json;

        json["width"] = graph.width;
        json["height"] = graph.height;
        json["nodes"] = nlohmann::json::array();
        json["edges"] = nlohmann::json::array();

        for (const Node& node : graph.nodes)
        {
            json["nodes"].push_back(serializeNode(
                node,
                graph,
                includeInternalMetadata
            ));
        }

        for (const Edge& edge : graph.edges)
        {
            json["edges"].push_back({
                { "source", edge.source },
                { "target", edge.target }
            });
        }

        std::ofstream file(path);
        if (!file.is_open())
        {
            throw std::runtime_error("Could not open output JSON file: " + path);
        }

        file << std::setw(2) << json << '\n';
    }
}

void JsonWriter::save(const Graph& graph, const std::string& path)
{
    saveGraph(
        graph,
        path,
        true
    );
}

void JsonWriter::saveSubmission(const Graph& graph, const std::string& path)
{
    saveGraph(
        graph,
        path,
        false
    );
}
