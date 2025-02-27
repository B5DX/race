#ifndef COMMON_H
#define COMMON_H

#include <vector>
#include <string>
#include <memory>
#include <utility>

using std::shared_ptr;
using std::make_shared;
using std::weak_ptr;
using std::vector;
using std::string;

struct Config {
    bool commutable = true;
    bool contraction = true;
    bool ESR = false;
    // bool Glore = false;
    bool scalar_replacement = false;
    bool vectorization = true;
    int max_leaves = INT32_MAX;
};

extern Config global_config;

#endif