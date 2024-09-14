#ifndef GRAPH_H
#define GRAPH_H

#include "common.h"
#include <algorithm>
#include <functional>
#include <iostream>
#include <list>
#include <map>

// template <typename T>
// struct PointerLess {
//     bool operator()(const T &lhs, const T &rhs) const { return *lhs < *rhs; }
// };

/**
 * @brief A undirected graph class. We assume the type Node is pointer.
 */
template <typename Node>
class UDGraph {
  private:
    vector<Node> nodes;
    vector<vector<Node>> adj_list;
    std::map<Node, size_t> node_to_index;
    // std::map<Node, size_t, PointerLess<Node>> node_to_index; // This is used for pointer comparison.

  public:
    UDGraph() {}

    inline void addNode(Node node) {
        nodes.push_back(node);
        adj_list.push_back(vector<Node>());
        node_to_index[node] = nodes.size() - 1;
    }

    void addEdge(Node src, Node dst) {
        auto src_iter = node_to_index.find(src);
        auto dst_iter = node_to_index.find(dst);
        if (src_iter == node_to_index.end() || dst_iter == node_to_index.end()) {
            throw std::runtime_error("Node not found in the graph.");
        }
        size_t src_idx = src_iter->second;
        size_t dst_idx = dst_iter->second;
        auto &src_list = adj_list[src_idx];
        auto &dst_list = adj_list[dst_idx];
        if (std::find(src_list.begin(), src_list.end(), dst) != src_list.end() ||
            std::find(dst_list.begin(), dst_list.end(), src) != dst_list.end()) {
            return;
        } else {
            src_list.push_back(dst);
            dst_list.push_back(src);
        }
    }

    void removeNode(Node node) {
        if (!hasNode(node)) {
            throw std::runtime_error("Node not found in the graph.");
        }
        size_t idx = node_to_index.at(node);
        nodes.erase(nodes.begin() + idx);
        adj_list.erase(adj_list.begin() + idx);
        for (vector<Node> &neighbors : adj_list) {
            neighbors.erase(std::remove(neighbors.begin(), neighbors.end(), node), neighbors.end());
        }
        node_to_index.erase(node);
        for (auto iter = nodes.begin() + idx; iter != nodes.end(); iter++) {
            --node_to_index[*iter];
        }
    }

    vector<Node> getNeighbors(Node node) const {
        if (!hasNode(node)) {
            throw std::runtime_error("Node not found in the graph.");
        }
        size_t idx = node_to_index.at(node);
        return adj_list[idx];
    }

    inline const vector<Node> &getNodes() const { return nodes; }

    size_t size() const { return nodes.size(); }

    void forEachNode(const std::function<void(Node)> &func) const {
        for (const auto &node : nodes) {
            func(node);
        }
    }

    void forEachNeighbor(Node node, const std::function<void(Node)> &func) const {
        for (const auto &neighbor : getNeighbors(node)) {
            func(neighbor);
        }
    }

    bool adjacent(Node src, Node dst) const {
        auto src_iter = node_to_index.find(src);
        auto dst_iter = node_to_index.find(dst);
        if (src_iter == node_to_index.end() || dst_iter == node_to_index.end()) {
            throw std::runtime_error("Node not found in the graph.");
        }
        size_t src_idx = src_iter->second;
        size_t dst_idx = dst_iter->second;
        auto &src_list = adj_list[src_idx];
        auto &dst_list = adj_list[dst_idx];
        return std::find(src_list.begin(), src_list.end(), dst) != src_list.end() ||
            std::find(dst_list.begin(), dst_list.end(), src) != dst_list.end();
    }

    inline bool hasNode(Node node) const {
        return node_to_index.find(node) != node_to_index.end();
    }

    void printGraph() const {
        for (size_t i = 0; i < nodes.size(); i++) {
            std::cout << i << ": ";
            std::cout << nodes[i]->to_string() << " -> ";
            for (const auto &neighbor : adj_list[i]) {
                std::cout << node_to_index.at(neighbor) << ", ";
            }
            std::cout << std::endl;
        }
    }
};

#endif