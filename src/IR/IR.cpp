#include <IR/IR.h>
#include "llvm/Support/Casting.h"

using std::shared_ptr;
using std::string;
using std::vector;
using namespace IR;

inline string generateParenthesesStr(const Expression *expr, const Operator *parent_op) {
    if (auto expr_op = llvm::dyn_cast<Operator>(expr)) {
        if (expr_op->get_priority() < parent_op->get_priority()) {
            return "(" + expr_op->to_string() + ")";
        } else {
            return expr->to_string();
        }
    } else {
        return expr->to_string();
    }
}

string UnaryOp::to_string() const {
    return op + generateParenthesesStr(expr.get(), this);
}

string SinOp::to_string() const {
    return "sin(" + expr->to_string() + ")";
}

string CosOp::to_string() const {
    return "cos(" + expr->to_string() + ")";
}

string BinaryOp::to_string() const {
    string lhs_str, rhs_str;
    lhs_str = generateParenthesesStr(lhs.get(), this);
    rhs_str = generateParenthesesStr(rhs.get(), this);
    return lhs_str + " " + op + " " + rhs_str;
}

string AssignOp::to_string() const {
    return lhs->to_string() + " = " + rhs->to_string();
}
