#ifndef GRAPH_H
#define GRAPH_H

#include "common.h"
#include <algorithm>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <queue>

/**
 * @brief A generic undirected graph class.
 */
template<typename Node>
class Graph {
  protected:
    vector<Node> nodes;
    vector<vector<size_t>> adj_list;
    std::map<Node, size_t> node_to_index;

  public:
    Graph() {}

    virtual inline void addNode(const Node &node) {
        if (hasNode(node)) {
            return;
        }
        nodes.push_back(node);
        adj_list.push_back(vector<size_t>());
        node_to_index[node] = nodes.size() - 1;
    }

    virtual inline void addNode(Node &&node) {
        if (hasNode(node)) {
            return;
        }
        nodes.emplace_back(node);
        adj_list.push_back(vector<size_t>());
        node_to_index[nodes.back()] = nodes.size() - 1;
    }

    virtual void addEdge(const Node &src, const Node &dst) {
        auto src_iter = node_to_index.find(src);
        auto dst_iter = node_to_index.find(dst);
        if (src_iter == node_to_index.end() || dst_iter == node_to_index.end()) {
            std::cerr << "addEdge Error: Node not found in graph.\n";
            throw std::runtime_error("Graph edge references an unknown node");
        }
        size_t src_idx = src_iter->second;
        size_t dst_idx = dst_iter->second;
        auto &src_list = adj_list[src_idx];
        if (std::find(src_list.begin(), src_list.end(), dst_idx) != src_list.end()) {
            return;
        } else {
            src_list.push_back(dst_idx);
        }
    }

    virtual void removeNode(const Node &node) {
        if (!hasNode(node)) {
            std::cerr << "removeNode Error: Node not found in graph.\n";
            throw std::runtime_error("Cannot remove an unknown graph node");
        }
        size_t del_idx = node_to_index.at(node);
        nodes.erase(nodes.begin() + del_idx);
        adj_list.erase(adj_list.begin() + del_idx);
        for (vector<size_t> &neighbors : adj_list) {
            neighbors.erase(std::remove(neighbors.begin(), neighbors.end(), del_idx), neighbors.end());
            for(size_t &neighbor : neighbors) {
                if (neighbor > del_idx) {
                    --neighbor;
                }
            }
        }
        node_to_index.erase(node);
        for (auto iter = nodes.begin() + del_idx; iter != nodes.end(); iter++) {
            --node_to_index[*iter];
        }
    }

    virtual vector<Node> getNeighbors(const Node &node) const {
        if (!hasNode(node)) {
            throw std::runtime_error("Cannot find neighbors of an unknown graph node");
        }
        size_t idx = node_to_index.at(node);
        vector<Node> neighbors;
        std::transform(adj_list[idx].begin(), adj_list[idx].end(), std::back_inserter(neighbors),
                       [this](size_t idx) { return this->nodes[idx]; });
        return neighbors;
    }

    virtual const vector<Node> &getNodes() const { return nodes; }

    virtual size_t size() const { return nodes.size(); }

    virtual void forEachNode(const std::function<void(Node)> &func) const {
        for (const auto &node : nodes) {
            func(node);
        }
    }

    virtual void forEachNeighbor(Node node, const std::function<void(Node)> &func) const {
        for (const auto &idx : adj_list[node_to_index.at(node)]) {
            func(nodes[idx]);
        }
    }

    virtual bool adjacent(const Node &src, const Node &dst) const {
        auto src_iter = node_to_index.find(src);
        auto dst_iter = node_to_index.find(dst);
        if (src_iter == node_to_index.end() || dst_iter == node_to_index.end()) {
            throw std::runtime_error("Cannot check adjacency of an unknown graph node");
        }
        size_t src_idx = src_iter->second;
        size_t dst_idx = dst_iter->second;
        auto &src_list = adj_list[src_idx];
        return std::find(src_list.begin(), src_list.end(), dst_idx) != src_list.end();
    }

    virtual inline bool hasNode(Node node) const {
        return node_to_index.find(node) != node_to_index.end();
    }

    vector<Node> getTopologicalOrder() const {
        vector<Node> res;
        vector<bool> visited(nodes.size(), false);
        std::function<void(size_t)> dfs = [&](size_t cur) {
            visited[cur] = true;
            for (size_t neighbor : adj_list[cur]) {
                if (!visited[neighbor]) {
                    dfs(neighbor);
                }
            }
            res.push_back(nodes[cur]);
        };
        for (size_t i = 0; i < nodes.size(); i++) {
            if (!visited[i]) {
                dfs(i);
            }
        }
        std::reverse(res.begin(), res.end());
        return res;
    }

};

/**
 * @brief A undirected graph class. We assume the type Node is pointer.
 */
template <typename Node>
class UDGraph : public Graph<Node> {
  public:
    UDGraph() : Graph<Node>() {}

    virtual void addEdge(const Node &src, const Node &dst) {
        auto src_iter = this->node_to_index.find(src);
        auto dst_iter = this->node_to_index.find(dst);
        if (src_iter == this->node_to_index.end() || dst_iter == this->node_to_index.end()) {
            throw std::runtime_error("Graph edge references an unknown node");
        }
        size_t src_idx = src_iter->second;
        size_t dst_idx = dst_iter->second;
        auto &src_list = this->adj_list[src_idx];
        auto &dst_list = this->adj_list[dst_idx];
        if (std::find(src_list.begin(), src_list.end(), dst_idx) != src_list.end() ||
            std::find(dst_list.begin(), dst_list.end(), src_idx) != dst_list.end()) {
            return;
        } else {
            src_list.push_back(dst_idx);
            dst_list.push_back(src_idx);
        }
    }

    virtual bool adjacent(const Node &src, const Node &dst) const {
        auto src_iter = this->node_to_index.find(src);
        auto dst_iter = this->node_to_index.find(dst);
        if (src_iter == this->node_to_index.end() || dst_iter == this->node_to_index.end()) {
            throw std::runtime_error("Cannot check adjacency of an unknown graph node");
        }
        size_t src_idx = src_iter->second;
        size_t dst_idx = dst_iter->second;
        auto &src_list = this->adj_list[src_idx];
        auto &dst_list = this->adj_list[dst_idx];
        return std::find(src_list.begin(), src_list.end(), dst_idx) != src_list.end() ||
            std::find(dst_list.begin(), dst_list.end(), src_idx) != dst_list.end();
    }

    UDGraph<Node> getSubGraph(const vector<Node> &nodes) const {
        UDGraph<Node> sub_graph;
        for (const auto &node : nodes) {
            sub_graph.addNode(node);
        }
        for (size_t i = 0; i < nodes.size(); i++) {
            for (size_t j = i + 1; j < nodes.size(); j++) {
                if (adjacent(nodes[i], nodes[j])) {
                    sub_graph.addEdge(nodes[i], nodes[j]);
                }
            }
        }
        return sub_graph;
    }

    vector<UDGraph<Node>> connectedComponents() const {
        vector<UDGraph<Node>> components;
        vector<bool> visited(this->nodes.size(), false);
        std::queue<size_t> q;
        vector<Node> component_nodes;
        for (size_t i = 0; i < this->nodes.size(); i++) {
            if (visited[i]) {
                continue;
            }
            component_nodes.clear();
            q.push(i);
            while (!q.empty()) {
                size_t cur = q.front();
                q.pop();
                if (visited[cur]) {
                    continue;
                }
                visited[cur] = true;
                component_nodes.push_back(this->nodes[cur]);
                for (size_t neighbor : this->adj_list[cur]) {
                    if (!visited[neighbor]) {
                        q.push(neighbor);
                    }
                }
            }
            components.push_back(getSubGraph(component_nodes));
        }
        return components;
    }

    void concatGraph(const UDGraph<Node> &other) {
        auto others_nodes = other.getNodes();
        for (const auto &node : others_nodes) {
            this->addNode(node);
        }
        for (size_t i = 0; i < others_nodes.size(); i++) {
            for (size_t j = i + 1; j < others_nodes.size(); j++) {
                if (other.adjacent(others_nodes[i], others_nodes[j])) {
                    addEdge(others_nodes[i], others_nodes[j]);
                }
            }
        }
    }

};

#endif
