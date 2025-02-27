#ifndef IVINFO_HPP
#define IVINFO_HPP

#include "IR/ExprTree.h"
#include "Solver/Graph.hpp"
#include "Solver/MatchSolver.h"

class RangeCircle;

/**
 * @brief IVInfo stores the information of Intermidiate or native ArrayRef. Number and DeclRef are also supported.
 * The object contains the following information:
 * 1. op: the operator of the expression, e.g. "+", "-", "*", "/".
 * 2. operands: the operands of the expression. If this is a native array, operands is empty.
 * 3. subs: the original (uncompressed) subscripts of each operand (The compressed subscript expressions will
 *    be computed by RangeCircle). If the operand is a DeclRef or Number, disp is empty.
 * 4. index_vars: the index_vars of the array. Used in generating expression.
 * 5. referenced_cnt: counts the number of times the array is referenced by other iv arrays.
 * 6. used_min_delta: the lower bounds of each dimension that the array can access.
 * 7. used_max_delta: the upper bounds of each dimension that the array can access.
 * 8. parent: the parent ivs that use this iv.
 * 9. range_circle: the range circle to which this iv belongs.
 *
 * For example, T[j][i] = A[2*i+1] + B[3*j][i-1], T is an intermediate array, A and B are native arrays. Then IVInfoManager will map T to an IVInfo object.
 * The object will be:
 * op: "+"
 * operands: {"A", "B"}
 * subs: {
 *  {(2*i+1)},
 *  {(1*i-1),(3*j+0)}
 * }
 * index_vars: {"i", "j"}
 *
 */
struct IVInfo {
    using Subs = vector<ir::Subscript>; // e.g. A[j+1][i+2] -> {(1*i+2), (1*j+1)}.
    string op = "";
    vector<string> operands; // assert(operands.size() == disp.size()). If operand[i] is DeclRef or Number, disp[i] is empty.
    vector<Subs> subs;
    vector<string> index_vars; // e.g. T[i][j] (=A[i]+B[j]) -> {"i", "j"}. Used in generating expression.
    int referenced_cnt = 0;

    // Record the dim-delta where this iv is used. Empty for native array, DeclRef and Number.
    // e.g. T is used T[j+1][i-1] and T[j-1][i], then used_max_delta["j"] = 1, used_min_delta["j"] = -1.
    std::map<string, int> used_min_delta;
    std::map<string, int> used_max_delta;

    // Record the parent ivs that use this iv.
    vector<string> parent;
    // Record the range circle to which this iv belongs.
    shared_ptr<RangeCircle> range_circle;
};

/**
 * @brief RangeCircle stores the information of a range circle which is used for array contraction.
 */
struct RangeCircle {
    // The iv arrays that resides in this range circle.
    std::set<string> iv_arrays;
    // Each range circle has a unique set of used_min_delta and used_max_delta. All the iv_arrays share the same delta.
    std::map<string, int> used_min_delta;
    std::map<string, int> used_max_delta;
    /**
     * @brief The index_mod map stores the information of the index's status.
     *
     * index_mod[idx_str] = 0 for original (keep the idx loop range unchanged when generating precompute loop),
     * 1 for compressed (eliminate the idx and don't generate this loop),
     * others (> 1) for the mod value (split the idx loop. Both prefetch loop and pipeline block will be generated).
     *
     * It is initialized to 0 for all the idx used by iv_arrays (which is the same as the key of used_min/max_delta).
     */
    std::map<string, unsigned int> index_mod;
    // Denote that this range circle should be inserted to the front of insert_loc.
    shared_ptr<ir::Block> insert_loc;

    RangeCircle(const std::set<string> &iv_arrays,
                const std::map<string, int> &used_max_delta, const std::map<string, int> &used_min_delta)
        : iv_arrays(iv_arrays), used_max_delta(used_max_delta), used_min_delta(used_min_delta) {
        insert_loc = nullptr;
        for (const auto &iter : used_max_delta) {
            index_mod[iter.first] = 0;
        }
    }

    RangeCircle(const std::set<string> &iv_arrays,
                const std::map<string, std::pair<int, int>> &packed_range)
        : iv_arrays(iv_arrays) {
        insert_loc = nullptr;
        for (const auto &iter : packed_range) {
            used_min_delta[iter.first] = iter.second.first;
            used_max_delta[iter.first] = iter.second.second;
        }
        for (const auto &iter : used_max_delta) {
            index_mod[iter.first] = 0;
        }
    }

    /**
     * @brief Get the expression subscript of the array with the circle information.
     * The array's subscript will be compressed or be taken modulo according to the circle information.
     *
     * @warning The function may return an empty vector if the array is compressed to a scalar.
     *
     * @param subs The original (uncompressed) subscript of the array.
     */
    vector<shared_ptr<ir::Expression>>
    getExprSubByCircle(const vector<ir::Subscript> &subs) const {
        vector<shared_ptr<ir::Expression>> expr_subs;
        for (const auto &sub : subs) {
            if (sub.isAipb()) {
                string idx_str = sub.getI()->to_string();
                unsigned int mod = this->index_mod.at(idx_str);
                if (mod == 1) { // compressed dimension
                    continue;
                } else if (mod == 0) { // original
                    expr_subs.push_back(sub.generateExpr());
                } else { // modulo dimension
                    auto temp_expr = sub.generateExpr();
                    expr_subs.push_back(make_shared<ir::ModOp>(temp_expr, make_shared<ir::Number>(mod)));
                }
            } else {
                expr_subs.push_back(sub.generateExpr());
            }
        }
        return expr_subs;
    }
};

class IVInfoManager {
private:
    std::map<string, IVInfo> iv_info_map;
    ir::ForStmt *parent_for;
    Graph<shared_ptr<RangeCircle>> range_circle_graph;
    using rational = boost::rational<int>;

    /**
     * @brief Build the range circle graph using the information of used_max_delta and used_min_delta.
     * The range information will be conveyed from the high level iv to the low level iv.
     *
     * @attention The function should be called after registerUsage(expr_node) which sets
     * the used_max_delta and used_min_delta of the highest level iv. Then the range information
     * is conveyed from the high level iv to the low level iv.
     *
     * @param explicit_iv The set of ivs that are retained. If it is empty, all intermediate variables are retained.
     */
    void buildRangeCircleGraph(std::set<string> explicit_iv = {});

public:
    IVInfoManager(ir::ForStmt *parent_for) : parent_for(parent_for) {}

    void addIVInfo(const string &iv_name, const IVInfo &iv_info) {
        iv_info_map[iv_name] = iv_info;
    }

    IVInfo getIVInfo(const string &iv_name) {
        return iv_info_map[iv_name];
    }

    void setParentFor(ir::ForStmt *parent_for) {
        this->parent_for = parent_for;
    }

    ir::ForStmt *getParentFor() {
        return parent_for;
    }

    bool hasIVInfo(const string &iv_name) {
        return iv_info_map.find(iv_name) != iv_info_map.end();
    }

    bool isNativeArray(const string &array) {
        return iv_info_map[array].operands.empty();
    }

    bool isArrayRef(const string &array) {
        for (const auto &sub : iv_info_map[array].subs) {
            if (!sub.empty()) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Compute the real operands information based on explict_iv.
     * 
     * @details If some ivs are eliminated, the expression of their parents will be expanded to reference their children.
     * So a new operands map can be computed based on the explicit_iv, which is more useful in array contraction.
     */
    std::map<string, vector<string>> getNewOperandsMap(std::set<string> explicit_iv);

    /**
     * @brief Generate the compressed precompute loop.
     *
     * @attention The function should be called after finish registerUsage().
     *
     * @param avn_block The AVNBlock to which the precompute loop is inserted.
     * @param ori_used_iv The set of ivs that are used by the original loop after extraction.
     * @param explicit_iv The set of ivs that will be explicitly created.
     */
    void generatePrecomputeLoop(shared_ptr<ir::AVNBlock> avn_block, std::set<string> ori_used_iv,
                                std::set<string> &explicit_iv);

    void arrayContraction(shared_ptr<ir::AVNBlock> avn_block, std::set<string> explicit_iv = {});

    Graph<shared_ptr<RangeCircle>> &getRangeCircleGraph() {
        return range_circle_graph;
    }

    const Graph<shared_ptr<RangeCircle>> &getRangeCircleGraph() const {
        return range_circle_graph;
    }

    void registerNativeArray(const shared_ptr<ir::ExprTree> &tree);

    // Register an iv-array with representing expression.
    void registerArray(string array, shared_ptr<DeltaNode> delta_node);

    // Register an used intermediate array after the extraction. This function is used for post-processing.
    void registerUsage(shared_ptr<ir::ExprTreeNode> expr_node);

    /**
     * @brief Get the ArrayRef or DeclRef Expression of an array with two operands.
     * e.g. T[j][i] (= A[i+1] + B[j][i-1]) is registered. getRefExpr("T", A[i], B[j+1][i-2]) will return T[j+1][i-1].
     */
    shared_ptr<ir::Expression> getRefExpr(string array, shared_ptr<DeltaNode> delta_node);

    /**
     * @brief Get the construct expression of an array after arrayContraction is done.
     *
     * @attention The function should be called after arrayContraction is done.
     */
    shared_ptr<ir::Expression> getContractedConstructLHS(const string &array, ir::ForStmt *parent_for, bool is_pipeline = false);

    /**
     * @brief Compute the construct expression of an array given shift parameter after arrayContraction is done.
     * e.g. T[j][i] (= A[i+1] + B[j][i-1]) is registered. getConstructRHS("T", {j:1}) will return A[i+1]+B[j+1][i-1].
     *
     * @attention The function should be called after arrayContraction is done.
     */
    shared_ptr<ir::Expression> getContractedConstructRHS(const string &array, ir::ForStmt *parent_for,
                                                         const std::map<string, int> &outer_const_disp, const std::set<string> &explicit_iv = {});

    /**
     * @brief Compute the construct expression of an array after arrayContraction is done.
     * The shift is automatically computed by the range circle information.
     *
     * @attention The function should be called after arrayContraction is done.
     *
     * @param is_pipeline If is_pipeline is true, this is used to generate the rhs in the pipeline block,
     * which means that the shift will be set to the maximum delta.
     */
    shared_ptr<ir::Expression> getContractedConstructRHS(const string &array, ir::ForStmt *parent_for, const std::set<string> &explicit_iv = {}, bool is_pipeline = false);

    shared_ptr<ir::Expression> getContractedArf(const string &array, ir::ForStmt *parent_for, const std::map<string, int> &shift_disp = {});

    std::map<string, int> getUsedMaxDelta(const string &array) {
        return iv_info_map[array].used_max_delta;
    }

    std::map<string, int> getUsedMinDelta(const string &array) {
        return iv_info_map[array].used_min_delta;
    }

    /**
     * @brief Get the topological order of the intermediate variables. The order is from
     * the highest level (iv used by the original loop after extraction) to
     * the lowest level (native).
     *
     * @param explicit_iv The set of ivs that are retained in the result. If it is empty, all intermediate variables are retained.
     */
    vector<string> getTopologicalOrder(std::set<string> explicit_iv = {});

    vector<string> getIndices(const string &array) {
        return iv_info_map[array].index_vars;
    }

    std::set<string> getIVArray() {
        std::set<string> ret;
        for (const auto &iter : iv_info_map) {
            if (!isNativeArray(iter.first)) {
                ret.insert(iter.first);
            }
        }
        return ret;
    }

    void printInfo();
};

#endif