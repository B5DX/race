#include "IR/IR.h"
#include "clang/ASTMatchers/ASTMatchers.h"

using namespace ir;

string ir::generateParenthesesStr(const Expression *expr, const Operator *parent_op,
                                  std::function<bool(int, int)> less_priority) {
    if (auto expr_op = llvm::dyn_cast<Operator>(expr)) {
        if (less_priority(expr_op->get_priority(), parent_op->get_priority())) {
            return "(" + expr_op->to_string() + ")";
        } else {
            return expr->to_string();
        }
    } else {
        return expr->to_string();
    }
}

string UnaryOp::to_string() const {
    return op + generateParenthesesStr(expr.get(), this, std::less_equal<int>());
}

string SinOp::to_string() const {
    return "sin(" + expr->to_string() + ")";
}

string CosOp::to_string() const {
    return "cos(" + expr->to_string() + ")";
}

string BinaryOp::to_string() const {
    string lhs_str, rhs_str;
    lhs_str = generateParenthesesStr(lhs.get(), this, std::less<int>());
    rhs_str = generateParenthesesStr(rhs.get(), this, std::less_equal<int>());
    return lhs_str + " " + op + " " + rhs_str;
}

string AssignOp::to_string() const {
    return lhs->to_string() + " = " + rhs->to_string();
}

shared_ptr<Expression> ir::parseExpression(const clang::Expr *expr, ForStmt *parent_loop) {
    if (auto bin_op = llvm::dyn_cast<clang::BinaryOperator>(expr)) {
        auto lhs = parseExpression(bin_op->getLHS());
        auto rhs = parseExpression(bin_op->getRHS());
        switch (bin_op->getOpcode()) {
        case clang::BinaryOperator::Opcode::BO_Mul:
            return std::make_shared<MulOp>(lhs, rhs);
        case clang::BinaryOperator::Opcode::BO_Div:
            return std::make_shared<DivOp>(lhs, rhs);
        case clang::BinaryOperator::Opcode::BO_Add:
            return std::make_shared<AddOp>(lhs, rhs);
        case clang::BinaryOperator::Opcode::BO_Sub:
            return std::make_shared<SubOp>(lhs, rhs);
        case clang::BinaryOperator::Opcode::BO_Assign:
            return std::make_shared<AssignOp>(lhs, rhs);
        default:
            throw std::runtime_error("Unsupported binary operator");
        }
    } else if (auto unary_op = llvm::dyn_cast<clang::UnaryOperator>(expr)) {
        auto sub_expr = parseExpression(unary_op->getSubExpr());
        switch (unary_op->getOpcode()) {
        case clang::UnaryOperator::Opcode::UO_Minus:
            return std::make_shared<NegOp>(sub_expr);
        default:
            throw std::runtime_error("Unsupported unary operator");
        }
    } else if (auto decl_ref = llvm::dyn_cast<clang::DeclRefExpr>(expr)) {
        return std::make_shared<DeclRef>(decl_ref->getNameInfo().getAsString());
    } else if (auto call_expr = llvm::dyn_cast<clang::CallExpr>(expr)) {
        auto callee = call_expr->getCallee();
        if (auto decl_ref = llvm::dyn_cast<clang::DeclRefExpr>(callee)) {
            if (decl_ref->getNameInfo().getAsString() == "sin") {
                auto arg = call_expr->getArg(0);
                return std::make_shared<SinOp>(parseExpression(arg));
            } else if (decl_ref->getNameInfo().getAsString() == "cos") {
                auto arg = call_expr->getArg(0);
                return std::make_shared<CosOp>(parseExpression(arg));
            }
        }
    } else if (auto integer_literal = llvm::dyn_cast<clang::IntegerLiteral>(expr)) {
        return std::make_shared<Number>(integer_literal->getValue().getSExtValue());
    } else if (auto floating_literal = llvm::dyn_cast<clang::FloatingLiteral>(expr)) {
        return std::make_shared<Number>(floating_literal->getValue().convertToDouble());
    } else if (auto implicit_cast = llvm::dyn_cast<clang::ImplicitCastExpr>(expr)) {
        return parseExpression(implicit_cast->getSubExpr());
    } else if (auto array_subscript = llvm::dyn_cast<clang::ArraySubscriptExpr>(expr)) {
        auto base = parseExpression(array_subscript->getBase());
        auto index = parseExpression(array_subscript->getIdx());
        vector<shared_ptr<Expression>> indices;
        indices.push_back(index);
        if (auto sub_array = llvm_pointer_cast<ir::ArrayRef>(base)) {
            // ! If sub_array->getIndices() return a copy instead of a reference, you can't write
            // ! indices.insert(indices.end(), sub_array->getIndices().begin(), sub_array->getIndices().end());
            // ! Because the two sub_array->getIndices() is different vector and the first one will be destroyed so that the begin iterator is invalid.
            auto sub_indices = sub_array->getIndices();
            indices.insert(indices.end(), sub_indices.begin(), sub_indices.end());
            return std::make_shared<ArrayRef>(sub_array->getArrayName(), indices, parent_loop);
        } else if (auto decl_ref = llvm_pointer_cast<ir::DeclRef>(base)) {
            return std::make_shared<ArrayRef>(decl_ref, indices, parent_loop);
        } else {
            throw std::runtime_error("Unsupported array expression");
        }
    } else if (auto paren_expr = llvm::dyn_cast<clang::ParenExpr>(expr)) {
        return parseExpression(paren_expr->getSubExpr());
    }
    expr->dump();
    throw std::runtime_error("Unsupported expression");
}

void ForStmt::buildBounds(const clang::ForStmt *for_stmt) {
    auto init = for_stmt->getInit();
    if (auto decl_stmt = llvm::dyn_cast<clang::DeclStmt>(init)) {
        for (auto decl : decl_stmt->decls()) {
            if (auto var_decl = llvm::dyn_cast<clang::VarDecl>(decl)) {
                loop_var = std::make_shared<DeclRef>(var_decl->getNameAsString());
                auto expr = var_decl->getInit();
                begin = parseExpression(expr);
            }
        }
    } else if (auto assign = llvm::dyn_cast<clang::BinaryOperator>(init)) {
        if (assign->getOpcode() != clang::BinaryOperator::Opcode::BO_Assign) {
            throw std::runtime_error("Invalid for loop initialization");
        }
        auto lhs = assign->getLHS();
        auto parsed_expr = parseExpression(lhs);
        loop_var = llvm_pointer_cast<DeclRef>(parsed_expr);
        if (!loop_var) {
            throw std::runtime_error("Invalid for loop initialization");
        }

        auto rhs = assign->getRHS();
        begin = parseExpression(rhs);
    }
    assert(loop_var && begin); // Make sure loop_var and begin are initialized.

    auto cond = for_stmt->getCond();
    if (auto bin_op = llvm::dyn_cast<clang::BinaryOperator>(cond)) {
        if (bin_op->getOpcode() == clang::BinaryOperator::Opcode::BO_LT) {
            auto lhs = bin_op->getLHS();
            assert(loop_var->to_string() == parseExpression(lhs)->to_string()); // Make sure the loop variable is on the left side of the comparison operator.
            auto rhs = bin_op->getRHS();
            end = std::make_shared<SubOp>(parseExpression(rhs), std::make_shared<Number>(1));
        } else if (bin_op->getOpcode() == clang::BinaryOperator::Opcode::BO_LE) {
            auto lhs = bin_op->getLHS();
            auto rhs = bin_op->getRHS();
            end = parseExpression(rhs);
        } else {
            throw std::runtime_error("Invalid for loop condition. Use comparison operator '<' or '<='.");
        }
    } else {
        throw std::runtime_error("Invalid for loop condition. Use comparison operator '<' or '<='.");
    }
    assert(end);

    // Make sure increment uses '++'.
    auto inc = for_stmt->getInc();
    if (auto unary_op = llvm::dyn_cast<clang::UnaryOperator>(inc)) {
        if (unary_op->getOpcode() == clang::UnaryOperator::Opcode::UO_PostInc) {
            // Do nothing
        } else if (unary_op->getOpcode() == clang::UnaryOperator::Opcode::UO_PreInc) {
            // Do nothing
        } else {
            throw std::runtime_error("Invalid for loop increment step. Use '++'.");
        }
    } else {
        throw std::runtime_error("Invalid for loop increment step. Use '++'.");
    }
}

void ForStmt::processInnerStmt(const clang::Stmt *stmt) {
    if (auto inner_for_stmt = llvm::dyn_cast<clang::ForStmt>(stmt)) {
        this->body.push_back(std::make_shared<ForStmt>(inner_for_stmt, this));
        // llvm::errs() << "Inner for loop detected\n";
    } else if (auto assign_expr = llvm::dyn_cast<clang::BinaryOperator>(stmt)) {
        if (assign_expr->getOpcode() == clang::BinaryOperator::Opcode::BO_Assign) {
            auto lhs = assign_expr->getLHS();
            auto rhs = assign_expr->getRHS();
            auto lhs_expr = parseExpression(lhs, this);
            auto rhs_expr = parseExpression(rhs, this);

            this->body.push_back(std::make_shared<ir::Assignment>(
                std::static_pointer_cast<ir::DeclRef>(lhs_expr), rhs_expr));
        }
    }
}

ForStmt::ForStmt(const clang::ForStmt *for_stmt, const ForStmt *parent_for) : Statement(STMT_FOR) {
    this->buildBounds(for_stmt);
    index_vars.push_back(loop_var);
    if (parent_for) {
        const auto &parent_index_vars = parent_for->index_vars;
        index_vars.insert(index_vars.end(), parent_index_vars.begin(), parent_index_vars.end());
        this->depth = parent_for->depth + 1;
    } else {
        this->depth = 0;
    }

    auto stmt_body = for_stmt->getBody();
    if (auto compound_stmt = llvm::dyn_cast<clang::CompoundStmt>(stmt_body)) {
        for (auto stmt : compound_stmt->body()) {
            processInnerStmt(stmt);
        }
    } else {
        processInnerStmt(stmt_body);
    }
}

string ForStmt::to_string() const {
    string indent = "";
    for (unsigned i = 0; i < depth; i++) {
        indent += "    ";
    }
    string ret = "for (" + loop_var->to_string() + " = " + begin->to_string() + " : " + end->to_string() + ") {\n";
    for (const auto &stmt : body) {
        ret += indent + "    " + stmt->to_string() + "\n";
    }
    ret += indent + "}";
    return ret;
}

vector<Block> ForStmt::getInnermostBlocks() const {
    vector<Block> ret;
    for (size_t idx = 0; idx < body.size();) {
        const auto &stmt = body[idx];
        if (auto inner_for = llvm_pointer_cast<ForStmt>(stmt)) {
            auto inner_blocks = inner_for->getInnermostBlocks();
            ret.insert(ret.end(), inner_blocks.begin(), inner_blocks.end());
            ++idx;
        } else if (llvm_isa<Assignment>(stmt)) {
            Block assign_block;
            do {
                assign_block.push_back(body[idx]);
                ++idx;
            } while (idx < body.size() && llvm_isa<Assignment>(body[idx]));
            ret.push_back(assign_block);
        }
    }
    return ret;
}

/**
 * @brief Parse the value of the constant term in the expression. e.g. (1-i -> 1). (2*(i-2) -> -4). (3*j+2 -> 2).
 * @warning Don't support DivOp or UnaryOp yet. Only support one loop var.
 * TODO: support multiple loop vars.
 */
int exprEval(shared_ptr<ir::Expression> expr) {
    if (auto num = llvm_pointer_cast<ir::Number>(expr)) {
        return std::get<0>(num->getValue());
    } else if (auto bop = llvm_pointer_cast<ir::BinaryOp>(expr)) {
        if (llvm_isa<ir::DivOp>(bop)) {
            throw std::runtime_error("DivOp is not supported yet.");
        }
        auto op = bop->getOpStr();
        if (llvm_isa<ir::DeclRef>(bop->getLHS())) { // We assume DeclRef must be loop var here.
            switch (op[0]) {
            case '-':
                return -exprEval(bop->getRHS());
            case '+':
                return exprEval(bop->getRHS());
            case '*':
                return 0;
            default:
                throw std::runtime_error("Unsupported binary operator.");
            }
        } else if (llvm_isa<ir::DeclRef>(bop->getRHS())) {
            switch (op[0]) {
            case '-':
                return exprEval(bop->getRHS());
            case '+':
                return exprEval(bop->getRHS());
            case '*':
                return 0;
            default:
                throw std::runtime_error("Unsupported binary operator.");
            }
        } else {
            switch (op[0]) {
            case '+':
                return exprEval(bop->getLHS()) + exprEval(bop->getRHS());
            case '-':
                return exprEval(bop->getLHS()) - exprEval(bop->getRHS());
            case '*':
                return exprEval(bop->getLHS()) * exprEval(bop->getRHS());
            default:
                throw std::runtime_error("Unsupported binary operator.");
            }
        }
    }
    // Only when there is only a loop var in the array index will the program execute to this elif.
    else if (auto decl_ref = llvm_pointer_cast<ir::DeclRef>(expr)) {
        return 0;
    } else {
        throw std::runtime_error("UnaryOp is not supported yet.");
    }
}

// TODO: Should be merged with exprEval for supporting multiple loop vars.
shared_ptr<DeclRef> searchDeclRef(shared_ptr<Expression> expr) {
    if (auto decl_ref = llvm_pointer_cast<DeclRef>(expr)) {
        return decl_ref;
    } else if (auto bop = llvm_pointer_cast<BinaryOp>(expr)) {
        auto lhs = searchDeclRef(bop->getLHS());
        if (lhs) {
            return lhs;
        } else {
            return searchDeclRef(bop->getRHS());
        }
    } else {
        return nullptr;
    }
}

// TODO: Should be merged with exprEval for supporting multiple loop vars.
std::map<string, int> ArrayRef::getDelta() const {
    std::map<string, int> ret;
    for (const auto &idx : indices) {
        if (auto decl_ref = searchDeclRef(idx)) {
            ret[decl_ref->to_string()] = exprEval(idx);
        }
    }
    return ret;
}

string ArrayRef::to_string() const {
    string ret = array->to_string();
    for (auto iter = indices.crbegin(); iter != indices.crend(); iter++) {
        ret += "[" + (*iter)->to_string() + "]";
    }
    return ret;
}