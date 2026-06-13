
#include <ogdf/basic/Graph.h>

#include <iostream>

int main()
{
    ogdf::Graph graph;

    const ogdf::node firstNode = graph.newNode();
    const ogdf::node secondNode = graph.newNode();
    const ogdf::node thirdNode = graph.newNode();

    graph.newEdge(firstNode, secondNode);
    graph.newEdge(secondNode, thirdNode);
    graph.newEdge(thirdNode, firstNode);

    std::cout << "OGDF probe graph created successfully.\n";
    std::cout << "Nodes: " << graph.numberOfNodes() << '\n';
    std::cout << "Edges: " << graph.numberOfEdges() << '\n';

    return 0;
}
