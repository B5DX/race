#include "IR/IR.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include <boost/rational.hpp>

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

string BinaryOp::to_string() const {
    string lhs_str, rhs_str;
    lhs_str = generateParenthesesStr(lhs.get(), this, std::less<int>());
    rhs_str = generateParenthesesStr(rhs.get(), this, std::less<int>());
    return lhs_str + " " + op + " " + rhs_str;
}

string AssignOp::to_string() const {
    return lhs->to_string() + " = " + rhs->to_string();
}

shared_ptr<Expression> ir::parseExpression(const clang::Expr *expr, ForStmt *parent_loop) {
    if (auto bin_op = llvm::dyn_cast<clang::BinaryOperator>(expr)) {
        auto lhs = parseExpression(bin_op->getLHS(), parent_loop);
        auto rhs = parseExpression(bin_op->getRHS(), parent_loop);
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
            exit(0);
        }
    } else if (auto unary_op = llvm::dyn_cast<clang::UnaryOperator>(expr)) {
        auto sub_expr = parseExpression(unary_op->getSubExpr(), parent_loop);
        switch (unary_op->getOpcode()) {
        case clang::UnaryOperator::Opcode::UO_Minus:
            return std::make_shared<NegOp>(sub_expr);
        default:
            exit(0);
        }
    } else if (auto decl_ref = llvm::dyn_cast<clang::DeclRefExpr>(expr)) {
        return std::make_shared<DeclRef>(decl_ref->getNameInfo().getAsString());
    } else if (auto call_expr = llvm::dyn_cast<clang::CallExpr>(expr)) {
        auto callee = call_expr->getCallee()->IgnoreImplicit();
        if (auto decl_ref = llvm::dyn_cast<clang::DeclRefExpr>(callee)) {
            if (decl_ref->getNameInfo().getAsString() == "sin") {
                auto arg = call_expr->getArg(0);
                auto sinop = std::make_shared<SinOp>(parseExpression(arg, parent_loop));
                return std::make_shared<CallOp>(sinop);
            } else if (decl_ref->getNameInfo().getAsString() == "cos") {
                auto arg = call_expr->getArg(0);
                auto cosop = std::make_shared<CosOp>(parseExpression(arg, parent_loop));
                return std::make_shared<CallOp>(cosop);
            }
        }
    } else if (auto integer_literal = llvm::dyn_cast<clang::IntegerLiteral>(expr)) {
        return std::make_shared<Number>(integer_literal->getValue().getSExtValue());
    } else if (auto floating_literal = llvm::dyn_cast<clang::FloatingLiteral>(expr)) {
        return std::make_shared<Number>(floating_literal->getValue().convertToDouble());
    } else if (auto implicit_cast = llvm::dyn_cast<clang::ImplicitCastExpr>(expr)) {
        return parseExpression(implicit_cast->getSubExpr(), parent_loop);
    } else if (auto array_subscript = llvm::dyn_cast<clang::ArraySubscriptExpr>(expr)) {
        auto base = parseExpression(array_subscript->getBase(), parent_loop);
        auto index = parseExpression(array_subscript->getIdx(), parent_loop);
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
            exit(0);
        }
    } else if (auto paren_expr = llvm::dyn_cast<clang::ParenExpr>(expr)) {
        return parseExpression(paren_expr->getSubExpr(), parent_loop);
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
                begin = parseExpression(expr, this);
            }
        }
    } else if (auto assign = llvm::dyn_cast<clang::BinaryOperator>(init)) {
        if (assign->getOpcode() != clang::BinaryOperator::Opcode::BO_Assign) {
            throw std::runtime_error("Invalid for loop initialization");
        }
        auto lhs = assign->getLHS();
        auto parsed_expr = parseExpression(lhs, this);
        loop_var = llvm_pointer_cast<DeclRef>(parsed_expr);
        if (!loop_var) {
            throw std::runtime_error("Invalid for loop initialization");
        }

        auto rhs = assign->getRHS();
        begin = parseExpression(rhs, this);
    }
    assert(loop_var && begin); // Make sure loop_var and begin are initialized.

    auto cond = for_stmt->getCond();
    if (auto bin_op = llvm::dyn_cast<clang::BinaryOperator>(cond)) {
        if (bin_op->getOpcode() == clang::BinaryOperator::Opcode::BO_LT) {
            auto lhs = bin_op->getLHS();
            assert(loop_var->to_string() == parseExpression(lhs, this)->to_string()); // Make sure the loop variable is on the left side of the comparison operator.
            auto rhs = bin_op->getRHS();
            end = std::make_shared<SubOp>(parseExpression(rhs, this), std::make_shared<Number>(1));
        } else if (bin_op->getOpcode() == clang::BinaryOperator::Opcode::BO_LE) {
            auto lhs = bin_op->getLHS();
            auto rhs = bin_op->getRHS();
            end = parseExpression(rhs, this);
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
        this->body->push_back(std::make_shared<ForStmt>(inner_for_stmt, this));
        // llvm::errs() << "Inner for loop detected\n";
    } else if (auto assign_expr = llvm::dyn_cast<clang::BinaryOperator>(stmt)) {
        if (assign_expr->getOpcode() == clang::BinaryOperator::Opcode::BO_Assign) {
            auto lhs = assign_expr->getLHS();
            auto rhs = assign_expr->getRHS();
            auto lhs_expr = parseExpression(lhs, this);
            auto rhs_expr = parseExpression(rhs, this);

            this->body->push_back(std::make_shared<ir::Assignment>(lhs_expr, rhs_expr));
        }
    }
}

ForStmt::ForStmt(const clang::ForStmt *for_stmt, ForStmt *parent_for) : Statement(STMT_FOR), parent_for(parent_for) {
    body = make_shared<ir::Block>(this);
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

// shared_ptr<Expression> ir::buildExpression(std::pair<string, int> idx) {
//     if (idx.second == 0) {
//         return std::make_shared<DeclRef>(idx.first);
//     } else {
//         if (idx.second > 0) {
//             return std::make_shared<AddOp>(std::make_shared<DeclRef>(idx.first), std::make_shared<Number>(idx.second));
//         } else {
//             return std::make_shared<SubOp>(std::make_shared<DeclRef>(idx.first), std::make_shared<Number>(-idx.second));
//         }
//     }
// }

shared_ptr<ir::Expression> ir::buildExpression(shared_ptr<ir::Expression> base_expr, int shift) {
    if (shift == 0) {
        return base_expr;
    } else if (shift > 0) {
        return std::make_shared<ir::AddOp>(base_expr, std::make_shared<ir::Number>(shift));
    } else {
        return std::make_shared<ir::SubOp>(base_expr, std::make_shared<ir::Number>(-shift));
    }
}

void ForStmt::recomputeArrayInfo() {
    for (auto &stmt : *body) {
        if (auto inner_for = llvm_pointer_cast<ForStmt>(stmt)) {
            inner_for->recomputeArrayInfo();
        } else if (auto assign = llvm_pointer_cast<Assignment>(stmt)) {
            // The rpi and norm_idx should be re-computed after the parent_for is set.
            auto recompute = [&](shared_ptr<ArrayRef> array_ref) {
                array_ref->recomputeArrayInfo();
            };
            for_each_expr<ArrayRef>(assign->getRHS(), recompute);
            for_each_expr<ArrayRef>(assign->getLHS(), recompute);
        }
    }
}

void ForStmt::setParentFor(ForStmt *parent_for, bool recompute_arr_info) {
    this->index_vars = {this->loop_var};
    if (parent_for) {
        const auto &parent_index_vars = parent_for->index_vars;
        index_vars.insert(index_vars.end(), parent_index_vars.begin(), parent_index_vars.end());
        this->depth = parent_for->depth + 1;
    }
    this->parent_for = parent_for;

    if (recompute_arr_info) {
        for (auto &stmt : *body) {
            if (auto inner_for = llvm_pointer_cast<ForStmt>(stmt)) {
                inner_for->setParentFor(this, recompute_arr_info);
            } else if (auto assign = llvm_pointer_cast<Assignment>(stmt)) {
                // The rpi and norm_idx should be re-computed after the parent_for is set.
                auto recompute = [&](shared_ptr<ArrayRef> array_ref) {
                    array_ref->recomputeArrayInfo();
                };
                for_each_expr<ArrayRef>(assign->getRHS(), recompute);
                for_each_expr<ArrayRef>(assign->getLHS(), recompute);
            }
        }
    } else {
        for (auto &stmt : *body) {
            if (auto inner_for = llvm_pointer_cast<ForStmt>(stmt)) {
                inner_for->setParentFor(this, recompute_arr_info);
            }
        }
    }
}

Subscript::Subscript(shared_ptr<ir::Expression> expr, const std::set<string> &loop_vars) {
    Subscript subs = is_aipb_form(expr, loop_vars);
    this->a = subs.a;
    this->i = subs.i;
    this->b = subs.b;
    this->expr_form = subs.expr_form;
}

/**
 * @brief Get the constant and the subscript from the two results of is_aipb_form.
 * Only one of the two results should be const_num.
 * The sign of the return is set corresponding to parameter subop.
 * So the caller can add the return directly without considering the sign for SubOp.
 *
 * @param subop If true, the operator is subtraction, and the sign of the return is set innerly.
 */
std::pair<int, Subscript> get_const_and_subscript(
    const Subscript &left_res, const Subscript &right_res, bool subop = false) {
    if (left_res.getExprForm() == Subscript::ExprForm::const_num) {
        Subscript tmp = right_res;
        if (subop) {
            tmp.setA(-tmp.getA());
            tmp.setB(-tmp.getB());
        }
        return {left_res.getB(), tmp};
    } else {
        if (subop)
            return {-right_res.getB(), left_res};
        else
            return {right_res.getB(), left_res};
    }
}

Subscript Subscript::is_aipb_form(shared_ptr<ir::Expression> expr, const std::set<string> &loop_vars) {
    using std::make_pair;
    auto invalid_res = Subscript(ExprForm::invalid, 1, nullptr, 0);
    if (ir::llvm_isa<ir::Number>(expr)) {
        return {
            ExprForm::const_num, 1, nullptr, int(std::get<0>(ir::llvm_pointer_cast<ir::Number>(expr)->getValue()))};
    } else if (auto decl_ref = ir::llvm_pointer_cast<ir::DeclRef>(expr)) {
        if (loop_vars.find(decl_ref->to_string()) != loop_vars.end()) {
            return {ExprForm::aipb, 1, decl_ref, 0};
        } else {
            return {ExprForm::nonloop_index_var, 1, decl_ref, 0};
        }
    }

    if (auto bop = ir::llvm_pointer_cast<ir::BinaryOp>(expr)) {
        Subscript left_res = is_aipb_form(bop->getLHS(), loop_vars);
        Subscript right_res = is_aipb_form(bop->getRHS(), loop_vars);
        int expr_id = left_res.expr_form + right_res.expr_form;
        if (ir::llvm_isa<ir::MulOp>(bop)) {
            if (expr_id == 11) {
                auto tmp = get_const_and_subscript(left_res, right_res);
                int coef = tmp.first;
                const Subscript &subs = tmp.second;
                return {ExprForm::aipb, coef * subs.a, subs.i, coef * subs.b};
            }
            if (expr_id == 20) {
                return {ExprForm::const_num, 1, nullptr, left_res.b * right_res.b};
            }
            if (expr_id == 110) {
                auto tmp = get_const_and_subscript(left_res, right_res);
                int coef = tmp.first;
                const Subscript &subs = tmp.second;
                return {ExprForm::nonloop_index_var, coef * subs.a, subs.i, coef * subs.b};
            }
            return invalid_res;
        } else if (ir::llvm_isa<ir::AddOp>(bop) || ir::llvm_isa<ir::SubOp>(bop)) {
            bool is_sub = ir::llvm_isa<ir::SubOp>(bop);
            if (expr_id == 2) {
                if (left_res.getI()->to_string() != right_res.getI()->to_string())
                    return invalid_res;
                if (is_sub)
                    return {ExprForm::aipb, left_res.a - right_res.a, left_res.i, left_res.b - right_res.b};
                else
                    return {ExprForm::aipb, left_res.a + right_res.a, left_res.i, left_res.b + right_res.b};
            }
            if (expr_id == 11 || expr_id == 110) {
                auto tmp = get_const_and_subscript(left_res, right_res, is_sub);
                int cons = tmp.first;
                const Subscript &subs = tmp.second;
                if (expr_id == 11)
                    return {ExprForm::aipb, subs.a, subs.i, subs.b + cons};
                else
                    return {ExprForm::nonloop_index_var, subs.a, subs.i, subs.b + cons};
            }
            if (expr_id == 20) {
                if (is_sub)
                    return {ExprForm::const_num, 1, nullptr, left_res.b - right_res.b};
                else
                    return {ExprForm::const_num, 1, nullptr, left_res.b + right_res.b};
            }
            return invalid_res;
        } else if (ir::llvm_isa<ir::DivOp>(bop)) {
            if (left_res.expr_form == ExprForm::aipb && right_res.expr_form == ExprForm::const_num) {
                auto tmp = get_const_and_subscript(left_res, right_res);
                int cons = tmp.first;
                const Subscript &subs = tmp.second;
                if (subs.a % cons != 0 || subs.b % cons != 0)
                    return invalid_res;
                return {ExprForm::aipb, subs.a / cons, subs.i, subs.b / cons};
            }
            if (left_res.expr_form == ExprForm::nonloop_index_var && right_res.expr_form == ExprForm::const_num) {
                auto tmp = get_const_and_subscript(left_res, right_res);
                int cons = tmp.first;
                const Subscript &subs = tmp.second;
                if (subs.a % cons != 0 || subs.b % cons != 0)
                    return invalid_res;
                return {ExprForm::nonloop_index_var, subs.a / cons, subs.i, subs.b / cons};
            }
            if (left_res.expr_form == ExprForm::const_num && right_res.expr_form == ExprForm::const_num) {
                if (left_res.b % right_res.b != 0)
                    return invalid_res;
                return {ExprForm::const_num, 1, nullptr, left_res.b / right_res.b};
            }
            return invalid_res;

        } else {
            llvm::outs() << "Unsupported binary operation: " << bop->to_string() << "\n";
            return invalid_res;
        }
    } else if (auto uop = ir::llvm_pointer_cast<ir::UnaryOp>(expr)) {
        if (ir::llvm_isa<ir::NegOp>(uop)) {
            auto tmp = is_aipb_form(uop->getOperand(), loop_vars);
            return {tmp.expr_form, -tmp.a, tmp.i, -tmp.b};
        } else {
            llvm::outs() << "Unsupported unary operation: " << uop->to_string() << "\n";
            return invalid_res;
        }
    }
    return invalid_res;
}

void ArrayRef::computeRPI() {
    using boost::rational;
    using ExprForm = ir::Subscript::ExprForm;
    vector<string> idx_list;
    vector<int> idx_coef;
    std::map<string, rational<int>> firstIdxOffset;
    std::map<string, vector<rational<int>>> idxDelta;
    for (const auto &idx : this->norm_idx) {
        if (!idx.isValid()) {
            rpi = 0;
            return;
        }
        ExprForm form = idx.getExprForm();
        if (form == ExprForm::nonloop_index_var) {
            idx_list.push_back(
                std::to_string(idx.getA()) + "*" + idx.getI()->to_string() + "+" + std::to_string(idx.getB()));
            idx_coef.push_back(idx.getA());
        } else if (form == ExprForm::const_num) {
            idx_list.push_back("#const#");
            idx_coef.push_back(idx.getB());
        } else { // form == ExprForm::aipb
            idx_list.push_back(idx.getI()->to_string());
            idx_coef.push_back(idx.getA());
            string i_str = idx.getI()->to_string();
            if (firstIdxOffset.find(i_str) == firstIdxOffset.end()) {
                firstIdxOffset[i_str] = rational<int>(idx.getB(), idx.getA());
                idxDelta[i_str].push_back(idx.getB() % idx.getA());
            } else {
                idxDelta[i_str].push_back(rational<int>(idx.getB(), idx.getA()) - firstIdxOffset[i_str]);
            }
        }
    }
    this->firstIdxOffset = firstIdxOffset;
    string compact_info = this->getArrayName()->to_string() + "$";
    for (size_t i = 0; i < idx_list.size(); i++) {
        compact_info += idx_list[i] + "$";
    }
    for (size_t i = 0; i < idx_coef.size(); i++) {
        compact_info += std::to_string(idx_coef[i]) + "$";
    }
    for (const auto &pair : idxDelta) {
        compact_info += pair.first + "$";
        for (const auto &delta : pair.second) {
            compact_info += std::to_string(delta.numerator()) + "/" + std::to_string(delta.denominator()) + "$";
        }
    }
    // If ESR is enabled, the firstIdxOffset of idx (except the innermost one) should be included in the compact_info.
    // Since different firstIdxOffset (except for innermost idx) is considered as different rpi array.
    if (global_config.ESR) {
        if (this->getParentLoop()) {
            string innermost_idx = this->getParentLoop()->getLoopVars().front()->to_string();
            for (auto &pair : firstIdxOffset) {
                if (pair.first != innermost_idx) {
                    compact_info += pair.first + "$" + std::to_string(pair.second.numerator()) + "/" + std::to_string(pair.second.denominator()) + "$";
                }
            }
        }
    }
    rpi = std::hash<string>()(compact_info);
}

void ArrayRef::computeNormIdx() {
    norm_idx.clear();
    std::set<string> loop_vars_set;
    if (parent_loop) {
        const auto &loop_vars = parent_loop->getLoopVars();
        for (const auto &loop_var : loop_vars) {
            loop_vars_set.insert(loop_var->to_string());
        }
    }
    for (const auto &idx : indices) {
        norm_idx.push_back(Subscript(idx, loop_vars_set));
    }
}

ArrayRef::ArrayRef(const string &array_name, const vector<Subscript> &subscripts, ForStmt *parent_loop)
    : Expression(EXPR_ARRAY_REF), norm_idx(subscripts), parent_loop(parent_loop) {
    this->array = std::make_shared<DeclRef>(array_name);
    // Compute the indices from norm_idx.
    for (const auto &idx : norm_idx) {
        if (!idx.isValid()) {
            llvm::outs() << "Invalid subscript: " << idx.to_string() << "\n";
            exit(0);
        }
        int a = idx.getA();
        int b = idx.getB();
        shared_ptr<DeclRef> i = nullptr;
        auto gene_expr = idx.generateExpr();
        if (gene_expr)
            this->indices.push_back(gene_expr);
        else {
            llvm::outs() << "Invalid subscript: " << idx.to_string() << "\n";
            exit(0);
        }
    }
    computeRPI();
}

ArrayRef::ArrayRef(const string &array_name, const vector<std::pair<string, int>> &indices, ForStmt *parent_loop)
    : Expression(EXPR_ARRAY_REF), parent_loop(parent_loop) {
    this->array = std::make_shared<DeclRef>(array_name);
    for (const auto &idx : indices) {
        this->indices.push_back(buildExpression(std::make_shared<DeclRef>(idx.first), idx.second));
    }
    computeNormIdx();
    computeRPI();
}

string ForStmt::to_string() const {
    string each_indent;
    for (unsigned i = 0; i < this->indent_space; i++) {
        each_indent += " ";
    }
    string indent = "";
    for (unsigned i = 0; i < depth; i++) {
        indent += each_indent;
    }
    string loop_var = this->loop_var->to_string();
    string ret = "for (" + loop_var + " = " + begin->to_string() + "; " 
                + loop_var + " <= " + end->to_string() + "; " 
                + loop_var + "++) {\n";
    for (const auto &stmt : *body) {
        ret += indent + each_indent + stmt->to_string() + "\n";
    }
    ret += indent + "}";
    return ret;
}

void Block::setParentFor(ForStmt *parent_for, bool recompute_arr_info) {
    this->parent_for = parent_for;
    for (auto &stmt : body) {
        if (auto inner_for = llvm_pointer_cast<ForStmt>(stmt)) {
            inner_for->setParentFor(parent_for);
        } else if (auto assign = llvm_pointer_cast<Assignment>(stmt)) {
            auto temp_func = [&](shared_ptr<ArrayRef> array_ref) {
                array_ref->setParentLoop(parent_for, recompute_arr_info);
            };
            for_each_expr<ArrayRef>(assign->getLHS(), temp_func);
            for_each_expr<ArrayRef>(assign->getRHS(), temp_func);
        }
    }
}

vector<Block> ForStmt::getInnermostBlocks() {
    vector<Block> ret;
    for (size_t idx = 0; idx < body->size();) {
        const auto &stmt = body->at(idx);
        if (auto inner_for = llvm_pointer_cast<ForStmt>(stmt)) {
            auto inner_blocks = inner_for->getInnermostBlocks();
            ret.insert(ret.end(), inner_blocks.begin(), inner_blocks.end());
            ++idx;
        } else if (llvm_isa<Assignment>(stmt)) {
            Block assign_block(this);
            do {
                assign_block.push_back(body->at(idx));
                ++idx;
            } while (idx < body->size() && llvm_isa<Assignment>(body->at(idx)));
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
int ir::exprEval(shared_ptr<ir::Expression> expr) {
    if (auto num = llvm_pointer_cast<ir::Number>(expr)) {
        return std::get<0>(num->getValue());
    } else if (auto bop = llvm_pointer_cast<ir::BinaryOp>(expr)) {
        if (llvm_isa<ir::DivOp>(bop)) {
            exit(0);
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
                exit(0);
            }
        } else if (llvm_isa<ir::DeclRef>(bop->getRHS())) {
            switch (op[0]) {
            case '-':
                return exprEval(bop->getLHS());
            case '+':
                return exprEval(bop->getLHS());
            case '*':
                return 0;
            default:
                exit(0);
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
                exit(0);
            }
        }
    }
    // Only when there is only a loop var in the array index will the program execute to this elif.
    else if (auto decl_ref = llvm_pointer_cast<ir::DeclRef>(expr)) {
        return 0;
    } else {
        exit(0);
    }
}

// TODO: Should be merged with exprEval for supporting multiple loop vars.
shared_ptr<DeclRef> ir::searchDeclRef(shared_ptr<Expression> expr) {
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

vector<std::pair<string, int>> ArrayRef::getOrderedDisp() const {
    vector<std::pair<string, int>> ret;
    for (const auto &idx : indices) {
        if (auto decl_ref = searchDeclRef(idx)) {
            ret.push_back({decl_ref->to_string(), exprEval(idx)});
        }
    }
    return ret;
}

// TODO: Should be merged with exprEval for supporting multiple loop vars.
std::map<string, int> ArrayRef::getDisp() const {
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
    ret += "[" + indices.back()->to_string();
    for (auto iter = indices.crbegin() + 1; iter != indices.crend(); iter++) {
        ret += "][" + (*iter)->to_string();
    }
    ret += "]";
    return ret;
}

shared_ptr<ir::BinaryOp> ir::buildBinaryOp(string op, shared_ptr<ir::Expression> l_expr, shared_ptr<ir::Expression> r_expr) {
    if (op == "+") {
        return std::make_shared<ir::AddOp>(l_expr, r_expr);
    } else if (op == "-") {
        return std::make_shared<ir::SubOp>(l_expr, r_expr);
    } else if (op == "*") {
        return std::make_shared<ir::MulOp>(l_expr, r_expr);
    } else if (op == "/") {
        return std::make_shared<ir::DivOp>(l_expr, r_expr);
    } else if (op == "call") {
        return std::make_shared<ir::CallOp>(l_expr, r_expr);
    } else if (op == "**") {
        return std::make_shared<ir::PowerOp>(l_expr, r_expr);
    } else if (op == "mod") {
        return std::make_shared<ir::ModOp>(l_expr, r_expr);
    } else {
        llvm::outs() << "Build unsupported binary operation: " << op << "\n";
        exit(0);
    }
}

shared_ptr<ir::UnaryOp> ir::buildUnaryOp(string op, shared_ptr<ir::Expression> expr) {
    if (op == "sin") {
        return std::make_shared<ir::SinOp>(expr);
    } else if (op == "cos") {
        return std::make_shared<ir::CosOp>(expr);
    } else if (op == "-") {
        return std::make_shared<ir::NegOp>(expr);
    } else {
        exit(0);
    }
}
