#ifndef IVINFO_HPP
#define IVINFO_HPP

#include "IR/ExprTree.h"

/**
 * @brief IVInfo stores the information of Intermidiate or native ArrayRef. Number and DeclRef are also supported.
 * The object contains the following information:
 * 1. op: the operator of the expression, e.g. "+", "-", "*", "/".
 * 2. operands: the operands of the expression. If this is a native array, operands is empty.
 * 3. disp: the displacement of each operand. If the operand is a DeclRef or Number, disp is empty.
 * 4. indices: the indices of the array. Used in generating expression.
 * 5. referenced_cnt: the count of references of the array .
 * 6. lower_bounds: the lower bounds of each dimension.
 * 7. upper_bounds: the upper bounds of each dimension.
 * 
 * For example, T[j][i] = A[i+1] + B[j][i-1], T is an intermediate array, A and B are native arrays. Then IVInfoManager will map T to an IVInfo object.
 * The object will be:
 * op: "+"
 * operands: {"A", "B"}
 * disp: {{("i", 1)}, {("i", -1), ("j", 0)}}
 * indices: {"i", "j"}
 */
struct IVInfo {
    // shared_ptr<ir::Expression> iv_expr = nullptr;
    using Disp = vector<std::pair<string, int>>; // e.g. A[j+1][i+2] -> {("i", 2), ("j", 1)}.
    string op;
    vector<string> operands; // assert(operands.size() == disp.size()). If operand[i] is DeclRef or Number, disp[i] is empty.
    vector<Disp> disp;
    vector<string> indices; // e.g. T[i][j] (=A[i]+B[j]) -> {"i", "j"}. Used in generating expression.
    int referenced_cnt;
    std::map<string, int> lower_bounds;
    std::map<string, int> upper_bounds;
};

class IVInfoManager {
private:
    std::map<string, IVInfo> iv_info_map;
    ir::ForStmt *parent_for;

public:
    IVInfoManager() {}
    // IVInfoManager(ir::ForStmt *parent_for) : parent_for(parent_for) {}

    void addIVInfo(const string &iv_name, const IVInfo &iv_info) {
        iv_info_map[iv_name] = iv_info;
    }

    IVInfo getIVInfo(const string &iv_name) {
        return iv_info_map[iv_name];
    }

    void setParentFor(ir::ForStmt *parent_for) {
        this->parent_for = parent_for;
    }

    bool hasIVInfo(const string &iv_name) {
        return iv_info_map.find(iv_name) != iv_info_map.end();
    }

    bool isNativeArray(const string &array) {
        return iv_info_map[array].operands.empty();
    }

    void registerNativeArray(const shared_ptr<ir::ExprTree> &tree);

    // Register an array with only one operand.
    void registerArray(string array, shared_ptr<ir::ExprTreeNode> sub_expr);
    
    // Register an array with two operands.
    void registerArray(string array, shared_ptr<ir::ExprTreeNode> l_expr, shared_ptr<ir::ExprTreeNode> r_expr);

    shared_ptr<ir::Expression> getExpression(string array, shared_ptr<ir::ExprTreeNode> l_expr_node, shared_ptr<ir::ExprTreeNode> r_expr_node);

    void printInfo();

    /**
     * @brief Works for ArrayRef, DeclRef and Number
     */
    static std::map<string, int> getDisp(const shared_ptr<ir::Expression> &expr) {
        if (auto arr_ref = ir::llvm_pointer_cast<ir::ArrayRef>(expr)) {
            return arr_ref->getDisp();
        } else {
            return std::map<string, int>();
        }
    }
};

#endif