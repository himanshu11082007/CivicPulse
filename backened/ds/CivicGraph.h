#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <queue>
#include <limits>
#include <cmath>
#include <algorithm>

/**
 * Fundamentals of Data Structures: Graph with Adjacency List & Dijkstra's Algorithm
 * 
 * Used for municipal fleet dispatch routing. Calculates the shortest path between
 * municipal depot / ward offices and prioritized complaint locations.
 */
class CivicGraph {
public:
    struct Edge {
        int to;
        double distanceKm; // Weight
        std::string streetName;
    };

    struct Vertex {
        int id;
        std::string name;
        double latitude;
        double longitude;
        bool isHub = false;
    };

    struct RouteResult {
        std::vector<int> pathNodeIds;
        std::vector<std::string> pathNodeNames;
        double totalDistanceKm = 0.0;
        int estimatedMinutes = 0;
    };

private:
    std::vector<Vertex> vertices;
    std::vector<std::vector<Edge>> adjList;
    std::unordered_map<std::string, int> nameToId;

    // Haversine formula to compute distance between two GPS coordinates in kilometers
    static double haversineDistance(double lat1, double lon1, double lat2, double lon2) {
        if (lat1 == 0.0 || lon1 == 0.0 || lat2 == 0.0 || lon2 == 0.0) {
            return 2.5; // Default average neighborhood distance
        }
        constexpr double R = 6371.0; // Earth radius in km
        constexpr double PI = 3.14159265358979323846;
        double dLat = (lat2 - lat1) * PI / 180.0;
        double dLon = (lon2 - lon1) * PI / 180.0;
        double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
                   std::cos(lat1 * PI / 180.0) * std::cos(lat2 * PI / 180.0) *
                   std::sin(dLon / 2) * std::sin(dLon / 2);
        double c = 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a));
        return R * c;
    }

public:
    CivicGraph() {
        initializeDefaultCityNetwork();
    }

    void initializeDefaultCityNetwork() {
        vertices.clear();
        adjList.clear();
        nameToId.clear();

        // Standard municipal zones & dispatch hubs (Centered around sample Indian city / Pune / Mumbai metro coordinates)
        addVertex("Municipal Central Depot", 18.5204, 73.8567, true);   // Node 0 (Depot)
        addVertex("Near school", 18.5246, 73.8512, false);              // Node 1
        addVertex("Near hospital", 18.5132, 73.8421, false);            // Node 2
        addVertex("Busy road", 18.5301, 73.8642, false);                // Node 3
        addVertex("Market area", 18.5188, 73.8530, false);              // Node 4
        addVertex("Residential area", 18.5080, 73.8610, false);         // Node 5
        addVertex("Public park", 18.5350, 73.8450, false);              // Node 6
        addVertex("College main gate", 18.5280, 73.8570, false);        // Node 7
        addVertex("West Ward Maintenance Center", 18.5120, 73.8350, true); // Node 8 (Depot)

        // Road network connections (bidirectional edges with real road distances in km)
        addEdge(0, 1, 1.4, "Vidyalaya Marg");
        addEdge(0, 4, 0.8, "City Center Avenue");
        addEdge(0, 3, 2.1, "Grand Trunk Highway");
        addEdge(1, 7, 0.9, "University Road");
        addEdge(1, 4, 1.1, "Station Link Road");
        addEdge(2, 4, 1.5, "Health Boulevard");
        addEdge(2, 8, 1.2, "West Ward Bypass");
        addEdge(4, 5, 1.6, "Green Park Lane");
        addEdge(3, 7, 1.3, "North Expressway");
        addEdge(6, 7, 1.7, "Botanical Garden Drive");
        addEdge(5, 8, 2.4, "Ring Road");
        addEdge(2, 5, 2.0, "South Arterial");
    }

    int addVertex(const std::string& name, double lat, double lon, bool isHub = false) {
        int id = static_cast<int>(vertices.size());
        vertices.push_back({id, name, lat, lon, isHub});
        adjList.emplace_back();
        nameToId[name] = id;
        return id;
    }

    void addEdge(int u, int v, double distKm, const std::string& street = "Connecting Road") {
        if (u >= 0 && u < static_cast<int>(vertices.size()) &&
            v >= 0 && v < static_cast<int>(vertices.size())) {
            adjList[u].push_back({v, distKm, street});
            adjList[v].push_back({u, distKm, street});
        }
    }

    int findNodeByLocation(const std::string& locationName) const {
        auto it = nameToId.find(locationName);
        if (it != nameToId.end()) {
            return it->second;
        }
        // Substring / partial matching
        for (const auto& v : vertices) {
            if (locationName.find(v.name) != std::string::npos ||
                v.name.find(locationName) != std::string::npos) {
                return v.id;
            }
        }
        return 0; // Default to Central Depot
    }

    int findNearestNode(double lat, double lon) const {
        if (vertices.empty()) return -1;
        int bestId = 0;
        double bestDist = std::numeric_limits<double>::max();

        for (const auto& v : vertices) {
            double d = haversineDistance(lat, lon, v.latitude, v.longitude);
            if (d < bestDist) {
                bestDist = d;
                bestId = v.id;
            }
        }
        return bestId;
    }

    // Dijkstra's Shortest Path Algorithm: O((V + E) log V)
    RouteResult dijkstra(int startId, int targetId) const {
        RouteResult result;
        int n = static_cast<int>(vertices.size());
        if (startId < 0 || startId >= n || targetId < 0 || targetId >= n) {
            return result;
        }

        std::vector<double> dist(n, std::numeric_limits<double>::infinity());
        std::vector<int> parent(n, -1);
        // Min-priority queue: pair of (distance, node)
        std::priority_queue<std::pair<double, int>,
                            std::vector<std::pair<double, int>>,
                            std::greater<std::pair<double, int>>> pq;

        dist[startId] = 0.0;
        pq.push({0.0, startId});

        while (!pq.empty()) {
            auto [currentDist, u] = pq.top();
            pq.pop();

            if (currentDist > dist[u]) continue;
            if (u == targetId) break;

            for (const auto& edge : adjList[u]) {
                int v = edge.to;
                double weight = edge.distanceKm;

                if (dist[u] + weight < dist[v]) {
                    dist[v] = dist[u] + weight;
                    parent[v] = u;
                    pq.push({dist[v], v});
                }
            }
        }

        if (dist[targetId] == std::numeric_limits<double>::infinity()) {
            return result; // No path
        }

        // Reconstruct path
        std::vector<int> path;
        for (int curr = targetId; curr != -1; curr = parent[curr]) {
            path.push_back(curr);
        }
        std::reverse(path.begin(), path.end());

        result.pathNodeIds = path;
        result.totalDistanceKm = dist[targetId];
        result.estimatedMinutes = static_cast<int>(std::ceil(result.totalDistanceKm / 25.0 * 60.0)) + 5; // 25 km/h city speed + 5 min dispatch overhead

        for (int id : path) {
            result.pathNodeNames.push_back(vertices[id].name);
        }

        return result;
    }

    // Multi-stop greedy TSP route for resolving prioritized complaints
    RouteResult calculateDispatchRoute(int hubId, const std::vector<int>& destinationNodes) const {
        RouteResult combined;
        if (destinationNodes.empty()) {
            return combined;
        }

        int current = hubId;
        std::vector<bool> visited(destinationNodes.size(), false);
        size_t visitedCount = 0;

        combined.pathNodeIds.push_back(current);
        combined.pathNodeNames.push_back(vertices[current].name);

        while (visitedCount < destinationNodes.size()) {
            int nearestIdx = -1;
            double nearestDist = std::numeric_limits<double>::infinity();
            RouteResult bestLeg;

            for (size_t i = 0; i < destinationNodes.size(); ++i) {
                if (visited[i]) continue;
                RouteResult leg = dijkstra(current, destinationNodes[i]);
                if (!leg.pathNodeIds.empty() && leg.totalDistanceKm < nearestDist) {
                    nearestDist = leg.totalDistanceKm;
                    nearestIdx = static_cast<int>(i);
                    bestLeg = leg;
                }
            }

            if (nearestIdx == -1) break; // unreachable

            visited[nearestIdx] = true;
            visitedCount++;

            // Append leg nodes (skip first since it matches current)
            for (size_t k = 1; k < bestLeg.pathNodeIds.size(); ++k) {
                combined.pathNodeIds.push_back(bestLeg.pathNodeIds[k]);
                combined.pathNodeNames.push_back(bestLeg.pathNodeNames[k]);
            }
            combined.totalDistanceKm += bestLeg.totalDistanceKm;
            current = destinationNodes[nearestIdx];
        }

        combined.estimatedMinutes = static_cast<int>(std::ceil(combined.totalDistanceKm / 25.0 * 60.0)) + static_cast<int>(destinationNodes.size() * 15);
        return combined;
    }

    const std::vector<Vertex>& getVertices() const {
        return vertices;
    }

    size_t getVertexCount() const {
        return vertices.size();
    }

    size_t getEdgeCount() const {
        size_t edges = 0;
        for (const auto& list : adjList) {
            edges += list.size();
        }
        return edges / 2; // undirected
    }
};
