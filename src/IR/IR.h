#ifndef IR_H
#define IR_H
#include "common.h"
#include "llvm/Support/Casting.h"
#include <boost/rational.hpp>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <variant>
#include <type_traits>

namespace clang {
class Expr;
class Stmt;
class ForStmt;
} // namespace clang

namespace ir {

enum PRIORITIES {
    PRIORITY_ASSIGN = 0,
    PRIORITY_ADD = 1,
    PRIORITY_MUL = 2,
    PRIORITY_UNARY = 3,
    PRIORITY_CALL = 4,
};

class Statement {
public:
    enum StmtKind {
        STMT_BLOCK,
        STMT_AVN_BLOCK,
        STMT_FOR,
        STMT_ASSIGN,
    };

private:
    const StmtKind kind;

public:
    StmtKind get_kind() const { return kind; }
    Statement(StmtKind kind) : kind(kind) {}

    virtual ~Statement() {}
    virtual string to_string() const = 0;
    virtual shared_ptr<Statement> clone() const = 0;
};

class Expression {
public:
    enum ExprKind {
        EXPR_COMMA,
        EXPR_OPERATOR,
        EXPR_UNARY,
        EXPR_NEGOP,
        EXPR_SINOP,
        EXPR_COSOP,
        EXPR_UNARY_LAST,
        EXPR_BINARY,
        EXPR_POWEROP,
        EXPR_MODOP,
        EXPR_ADDOP,
        EXPR_SUBOP,
        EXPR_MULOP,
        EXPR_DIVOP,
        EXPR_ASSIGN,
        EXPR_CALLOP,
        EXPR_BINARY_LAST,
        EXPR_OPERATOR_LAST,
        EXPR_NUMBER,
        EXPR_DECL_REF,
        EXPR_ARRAY_REF,
        EXPR_INTER_VAR,
        EXPR_ARRAY_REF_LAST,
    };

private:
    const ExprKind kind;

public:
    ExprKind get_kind() const { return kind; }
    Expression(ExprKind kind) : kind(kind) {}

    virtual ~Expression() {}
    virtual string to_string() const = 0;
    virtual shared_ptr<Expression> clone() const = 0;
};

template <typename To, typename From>
inline std::shared_ptr<To> llvm_pointer_cast(const std::shared_ptr<From> &e) {
    if (llvm::isa<To>(e.get())) {
        return std::static_pointer_cast<To>(e);
    } else {
        return nullptr;
    }
}

template <typename To, typename From>
inline bool llvm_isa(const std::shared_ptr<From> &e) {
    return llvm::isa<To>(e.get());
}

class Operator : public Expression {
protected:
    const string op;

public:
    Operator(ExprKind kind) : Expression(kind) {}
    Operator(ExprKind kind, const string &op) : Expression(kind), op(op) {}

    virtual ~Operator() {}
    virtual string to_string() const = 0;
    virtual PRIORITIES get_priority() const = 0;
    virtual string getOpStr() const { return op; }

    static bool classof(const Expression *expr) {
        return expr->get_kind() >= EXPR_OPERATOR && expr->get_kind() < EXPR_OPERATOR_LAST;
    }

    virtual shared_ptr<Expression> clone() const = 0;
};

class UnaryOp : public Operator {
protected:
    shared_ptr<Expression> expr;

public:
    UnaryOp(ExprKind kind, const string &op, shared_ptr<Expression> expr)
        : Operator(kind, op), expr(expr) {}
    virtual PRIORITIES get_priority() const override { return PRIORITY_UNARY; }
    virtual string to_string() const override;

    virtual shared_ptr<Expression> getOperand() const { return expr; }
    virtual void setOperand(shared_ptr<Expression> expr) { this->expr = expr; }

    static bool classof(const Expression *expr) {
        return expr->get_kind() >= EXPR_UNARY && expr->get_kind() < EXPR_UNARY_LAST;
    }
};

class NegOp : public UnaryOp {
public:
    NegOp(shared_ptr<Expression> expr) : UnaryOp(EXPR_NEGOP, "-", expr) {}

    static bool classof(const Expression *expr) {
        return expr->get_kind() == EXPR_NEGOP;
    }

    virtual string to_string() const override {
        return "(" + UnaryOp::to_string() + ")";
    }

    virtual shared_ptr<Expression> clone() const override {
        return std::make_shared<NegOp>(expr->clone());
    }
};

class SinOp : public UnaryOp {
public:
    SinOp(shared_ptr<Expression> expr) : UnaryOp(EXPR_SINOP, "sin", expr) {}
    virtual string to_string() const override {
        return "sin";
    }

    static bool classof(const Expression *expr) {
        return expr->get_kind() == EXPR_SINOP;
    }

    virtual shared_ptr<Expression> clone() const override {
        return std::make_shared<SinOp>(expr->clone());
    }
};

class CosOp : public UnaryOp {
public:
    CosOp(shared_ptr<Expression> expr) : UnaryOp(EXPR_COSOP, "cos", expr) {}
    virtual string to_string() const override {
        return "cos";
    }

    static bool classof(const Expression *expr) {
        return expr->get_kind() == EXPR_COSOP;
    }

    virtual shared_ptr<Expression> clone() const override {
        return std::make_shared<CosOp>(expr->clone());
    }
};

class BinaryOp : public Operator {
protected:
    shared_ptr<Expression> lhs, rhs;

public:
    BinaryOp(ExprKind kind, const string &op, shared_ptr<Expression> lhs, shared_ptr<Expression> rhs)
        : Operator(kind, op), lhs(lhs), rhs(rhs) {}

    virtual string to_string() const override;

    virtual void setLHS(shared_ptr<Expression> lhs) { this->lhs = lhs; }
    virtual void setRHS(shared_ptr<Expression> rhs) { this->rhs = rhs; }
    virtual shared_ptr<Expression> getLHS() const { return lhs; }
    virtual shared_ptr<Expression> getRHS() const { return rhs; }

    static bool classof(const Expression *expr) {
        return expr->get_kind() >= EXPR_BINARY && expr->get_kind() < EXPR_BINARY_LAST;
    }
};

class PowerOp : public BinaryOp {
public:
    PowerOp(shared_ptr<Expression> lhs, shared_ptr<Expression> rhs) : BinaryOp(EXPR_POWEROP, "**", lhs, rhs) {}

    virtual PRIORITIES get_priority() const override { return PRIORITY_CALL; }

    static bool classof(const Expression *expr) {
        return expr->get_kind() == EXPR_POWEROP;
    }

    virtual shared_ptr<Expression> clone() const override {
        return std::make_shared<PowerOp>(lhs->clone(), rhs->clone());
    }
};

class ModOp : public BinaryOp {
public:
    ModOp(shared_ptr<Expression> lhs, shared_ptr<Expression> rhs) : BinaryOp(EXPR_MODOP, "%", lhs, rhs) {}

    virtual PRIORITIES get_priority() const override { return PRIORITY_MUL; }

    static bool classof(const Expression *expr) {
        return expr->get_kind() == EXPR_MODOP;
    }

    virtual shared_ptr<Expression> clone() const override {
        return std::make_shared<ModOp>(lhs->clone(), rhs->clone());
    }
};

class AddOp : public BinaryOp {
public:
    AddOp() : BinaryOp(EXPR_ADDOP, "+", nullptr, nullptr) {}
    AddOp(shared_ptr<Expression> lhs, shared_ptr<Expression> rhs) : BinaryOp(EXPR_ADDOP, "+", lhs, rhs) {}

    virtual PRIORITIES get_priority() const override { return PRIORITY_ADD; }

    static bool classof(const Expression *expr) {
        return expr->get_kind() == EXPR_ADDOP;
    }

    virtual shared_ptr<Expression> clone() const override {
        return std::make_shared<AddOp>(lhs->clone(), rhs->clone());
    }
};

class SubOp : public BinaryOp {
public:
    SubOp() : BinaryOp(EXPR_SUBOP, "-", nullptr, nullptr) {}
    SubOp(shared_ptr<Expression> lhs, shared_ptr<Expression> rhs) : BinaryOp(EXPR_SUBOP, "-", lhs, rhs) {}

    virtual PRIORITIES get_priority() const override { return PRIORITY_ADD; }

    static bool classof(const Expression *expr) {
        return expr->get_kind() == EXPR_SUBOP;
    }

    virtual shared_ptr<Expression> clone() const override {
        return std::make_shared<SubOp>(lhs->clone(), rhs->clone());
    }
};

class MulOp : public BinaryOp {
public:
    MulOp() : BinaryOp(EXPR_MULOP, "*", nullptr, nullptr) {}
    MulOp(shared_ptr<Expression> lhs, shared_ptr<Expression> rhs) : BinaryOp(EXPR_MULOP, "*", lhs, rhs) {}

    virtual PRIORITIES get_priority() const override { return PRIORITY_MUL; }

    static bool classof(const Expression *expr) {
        return expr->get_kind() == EXPR_MULOP;
    }

    virtual shared_ptr<Expression> clone() const override {
        return std::make_shared<MulOp>(lhs->clone(), rhs->clone());
    }
};

class DivOp : public BinaryOp {
public:
    DivOp() : BinaryOp(EXPR_DIVOP, "/", nullptr, nullptr) {}
    DivOp(shared_ptr<Expression> lhs, shared_ptr<Expression> rhs) : BinaryOp(EXPR_DIVOP, "/", lhs, rhs) {}

    virtual PRIORITIES get_priority() const override { return PRIORITY_MUL; }

    static bool classof(const Expression *expr) {
        return expr->get_kind() == EXPR_DIVOP;
    }

    virtual shared_ptr<Expression> clone() const override {
        return std::make_shared<DivOp>(lhs->clone(), rhs->clone());
    }
};

class AssignOp : public BinaryOp {
public:
    AssignOp() : BinaryOp(EXPR_ASSIGN, "=", nullptr, nullptr) {}
    AssignOp(shared_ptr<Expression> lhs, shared_ptr<Expression> rhs) : BinaryOp(EXPR_ASSIGN, "=", lhs, rhs) {}

    virtual PRIORITIES get_priority() const override { return PRIORITY_ASSIGN; }

    virtual string to_string() const override;

    static bool classof(const Expression *expr) {
        return expr->get_kind() == EXPR_ASSIGN;
    }

    virtual shared_ptr<Expression> clone() const override {
        return std::make_shared<AssignOp>(lhs->clone(), rhs->clone());
    }
};

class DeclRef : public Expression {
protected:
    string name;

public:
    DeclRef(const string &name) : Expression(EXPR_DECL_REF), name(name) {}

    virtual string to_string() const override { return name; }

    static bool classof(const Expression *expr) {
        return expr->get_kind() == EXPR_DECL_REF;
    }

    bool operator==(const DeclRef &other) const {
        return name == other.name;
    }

    bool operator<(const DeclRef &other) const {
        return name < other.name;
    }

    virtual shared_ptr<Expression> clone() const override {
        return std::make_shared<DeclRef>(name);
    }
};

class Number : public Expression {
    using LL = long long;

protected:
    std::variant<LL, double> value;

public:
    Number(LL value) : Expression(EXPR_NUMBER), value(value) {}
    Number(double value) : Expression(EXPR_NUMBER), value(value) {}

    Number(int value) : Expression(EXPR_NUMBER), value(static_cast<LL>(value)) {}
    Number(long value) : Expression(EXPR_NUMBER), value(static_cast<LL>(value)) {}
    Number(unsigned value) : Expression(EXPR_NUMBER), value(static_cast<LL>(value)) {}
    Number(float value) : Expression(EXPR_NUMBER), value(static_cast<double>(value)) {}

    std::variant<LL, double> getValue() const { return value; }

    virtual string to_string() const override {
        if (std::holds_alternative<LL>(value)) {
            LL temp = std::get<LL>(value);
            if (temp < 0) {
                return "(" + std::to_string(temp) + ")";
            } else {
                return std::to_string(temp);
            }
        }
        else {
            double temp = std::get<double>(value);
            if (temp < 0) {
                return "(" + std::to_string(temp) + ")";
            } else {
                return std::to_string(temp);
            }
        }
    }

    static bool classof(const Expression *expr) {
        return expr->get_kind() == EXPR_NUMBER;
    }

    virtual shared_ptr<Expression> clone() const override {
        if (std::holds_alternative<LL>(value))
            return std::make_shared<Number>(std::get<LL>(value));
        else
            return std::make_shared<Number>(std::get<double>(value));
    }
};

class Assignment : public Statement {
protected:
    shared_ptr<AssignOp> assign;

public:
    Assignment(shared_ptr<Expression> lhs, shared_ptr<Expression> rhs) : Statement(STMT_ASSIGN) {
        assign = std::make_shared<AssignOp>(lhs, rhs);
    }

    virtual string to_string() const override { return assign->getLHS()->to_string() + " = " + assign->getRHS()->to_string() + ";"; }
    virtual shared_ptr<AssignOp> getAssign() const { return assign; }
    virtual void setAssign(shared_ptr<AssignOp> assign) { this->assign = assign; }
    virtual shared_ptr<Expression> getLHS() const { return assign->getLHS(); }
    virtual shared_ptr<Expression> getRHS() const { return assign->getRHS(); }
    virtual void setLHS(shared_ptr<Expression> lhs) { assign->setLHS(lhs); }
    virtual void setRHS(shared_ptr<Expression> rhs) { assign->setRHS(rhs); }

    static bool classof(const Statement *stmt) { return stmt->get_kind() == STMT_ASSIGN; }
    virtual shared_ptr<Statement> clone() const override {
        return std::make_shared<Assignment>(assign->getLHS()->clone(), assign->getRHS()->clone());
    }
};

class CommaExpr : public Expression {
protected:
    vector<shared_ptr<Expression>> exprs;

public:
    CommaExpr() : Expression(EXPR_COMMA) {}
    CommaExpr(const vector<shared_ptr<Expression>> &exprs) : Expression(EXPR_COMMA), exprs(exprs) {}

    virtual string to_string() const override {
        string ret = exprs[0]->to_string();
        for (size_t i = 1; i < exprs.size(); i++) {
            ret += ", " + exprs[i]->to_string();
        }
        return ret;
    }

    static bool classof(const Expression *expr) {
        return expr->get_kind() == EXPR_COMMA;
    }

    virtual shared_ptr<Expression> clone() const override {
        vector<shared_ptr<Expression>> new_exprs;
        for (const auto &expr : exprs) {
            new_exprs.push_back(expr->clone());
        }
        return std::make_shared<CommaExpr>(new_exprs);
    }
};

/**
 * Example:
 *      call
 *     /   \         <=> sin(A[i][j])
 *   sin    A[i][j]
 */

class CallOp : public BinaryOp {
public:
    CallOp() : BinaryOp(EXPR_CALLOP, "call", nullptr, nullptr) {}
    CallOp(shared_ptr<Expression> lhs, shared_ptr<Expression> rhs) : BinaryOp(EXPR_CALLOP, "call", lhs, rhs) {}
    CallOp(shared_ptr<SinOp> sinop) : BinaryOp(EXPR_CALLOP, "call", sinop, sinop->getOperand()) {}
    CallOp(shared_ptr<CosOp> cosop) : BinaryOp(EXPR_CALLOP, "call", cosop, cosop->getOperand()) {}

    virtual PRIORITIES get_priority() const override { return PRIORITY_CALL; }

    virtual string to_string() const override { return lhs->to_string() + "(" + rhs->to_string() + ")"; }

    static bool classof(const Expression *expr) {
        return expr->get_kind() == EXPR_CALLOP;
    }

    virtual shared_ptr<Expression> clone() const override {
        return std::make_shared<CallOp>(lhs->clone(), rhs->clone());
    }
};

class ForStmt;

class Block : public Statement {
protected:
    vector<shared_ptr<Statement>> body;
    ForStmt *parent_for;

public:
    Block(ForStmt *parent_for) : Statement(STMT_BLOCK), parent_for(parent_for) {}
    Block(const Block &block) : Statement(STMT_BLOCK), body(block.body), parent_for(block.parent_for) {}
    Block(Block &&block) : Statement(STMT_BLOCK), body(std::move(block.body)), parent_for(block.parent_for) {}

    Block &operator=(const Block &block) {
        body = block.body;
        parent_for = block.parent_for;
        return *this;
    }

    Block &operator=(Block &&block) {
        body = std::move(block.body);
        parent_for = block.parent_for;
        return *this;
    }

    ForStmt *getParentFor() { return parent_for; }
    void setParentFor(ForStmt *parent_for, bool recompute_arr_info = false);
    const vector<shared_ptr<Statement>> &getBody() const { return body; }
    vector<shared_ptr<Statement>> &getBody() { return body; }

    static bool classof(const Statement *stmt) { return stmt->get_kind() == STMT_BLOCK; }
    virtual string to_string() const override {
        string ret;
        if (body.empty())
            return ret;
        ret = body[0]->to_string();
        for (size_t i = 1; i < body.size(); i++) {
            ret += "\n" + body[i]->to_string();
        }
        return ret;
    }

    auto begin() { return body.begin(); }
    auto end() { return body.end(); }
    auto begin() const { return body.begin(); }
    auto end() const { return body.end(); }
    void push_back(shared_ptr<Statement> stmt) { body.push_back(stmt); }
    size_t size() const { return body.size(); }
    const shared_ptr<Statement> &operator[](size_t idx) const { return body[idx]; }
    shared_ptr<Statement> &operator[](size_t idx) { return body[idx]; }
    bool empty() const { return body.empty(); }
    void push_front(shared_ptr<Statement> stmt) { body.insert(body.begin(), stmt); }
    shared_ptr<Statement> &front() { return body.front(); }
    shared_ptr<Statement> &at(size_t idx) { return body.at(idx); }
    virtual shared_ptr<Statement> clone() const override {
        auto new_block = std::make_shared<Block>(this->parent_for);
        for (const auto &stmt : this->body) {
            new_block->push_back(stmt->clone());
        }
        return new_block;
    }
};

class ForStmt : public Statement {
    // Parse and set the loop variable, begin, and end from the for statement.
    void buildBounds(const clang::ForStmt *for_stmt);
    void processInnerStmt(const clang::Stmt *stmt);

protected:
    shared_ptr<DeclRef> loop_var;
    shared_ptr<Expression> begin, end; // begin <= loop_var <= end
    shared_ptr<Block> body;
    /**
     * @brief The front element is the innermost index.
     * e.g. for(j...) {for(i...) {}} -> index_vars = {i, j}.
     */
    vector<shared_ptr<DeclRef>> index_vars;
    ForStmt *parent_for;
    unsigned depth;
    size_t indent_space = 4;

public:
    /**
     * @brief Construct a ForStmt object from a clang::ForStmt object.
     * @warning Parent_for cannot be shared_ptr<ForStmt> because of the constructor uses this.
     * And shared_from_this() can only be called after the object is constructed.
     */
    ForStmt(const clang::ForStmt *for_stmt, ForStmt *parent_for = nullptr);
    ForStmt(shared_ptr<DeclRef> loop_var, shared_ptr<Expression> begin, shared_ptr<Expression> end, ForStmt *parent_for)
        : Statement(STMT_FOR), loop_var(loop_var), begin(begin), end(end), parent_for(const_cast<ForStmt *>(parent_for)), depth(parent_for ? parent_for->depth + 1 : 0) {
        this->body = std::make_shared<Block>(this);
        index_vars.push_back(this->loop_var);
        if (parent_for) {
            const auto &parent_index_vars = parent_for->index_vars;
            index_vars.insert(index_vars.end(), parent_index_vars.begin(), parent_index_vars.end());
        }
    }
    ForStmt(string loop_var, shared_ptr<Expression> begin, shared_ptr<Expression> end, ForStmt *parent_for)
        : ForStmt(make_shared<DeclRef>(loop_var), begin, end, parent_for) {}

    static bool classof(const Statement *stmt) { return stmt->get_kind() == STMT_FOR; }
    virtual string to_string() const override;

    // TODO: Block may be defined as a class named AssignBlock inherited from Statement, which can contain the loop_vars information for ArrayRef's index.
    vector<Block> getInnermostBlocks();

    const vector<shared_ptr<DeclRef>> &getLoopVars() const {
        return index_vars;
    }

    /**
     * @brief the parent_for of the for statement and recompute the array information for
     * the array reference in the for statement if recompute_arr_info is true.
     */
    void setParentFor(ForStmt *parent_for, bool recompute_arr_info = true);

    /**
     * @brief Recursively compute the array information (rpi and norm_idx) for the array reference in the for statement.
     */
    void recomputeArrayInfo();

    void setDepth(unsigned depth) {
        this->depth = depth;
        for (auto &stmt : *body) {
            if (auto inner_for = llvm_pointer_cast<ForStmt>(stmt)) {
                inner_for->setDepth(this->depth + 1);
            }
        }
    }
    void setIndentSpace(size_t space) {
        indent_space = space;
        for (auto &stmt : *body) {
            if (auto inner_for = llvm_pointer_cast<ForStmt>(stmt)) {
                inner_for->setIndentSpace(space);
            }
        }
    }
    ForStmt *getParentFor() { return parent_for; }
    shared_ptr<DeclRef> getLoopVar() const { return loop_var; }
    shared_ptr<Expression> getBegin() const { return begin; }
    shared_ptr<Expression> getEnd() const { return end; }
    const shared_ptr<Block> &getBody() const { return body; }
    shared_ptr<Block> getBody() { return body; }
    void setBody(shared_ptr<Block> body) { this->body = body; }
    virtual shared_ptr<Statement> clone() const override {
        auto new_for = std::make_shared<ir::ForStmt>(loop_var, begin, end, parent_for);
        new_for->setDepth(depth);
        new_for->setIndentSpace(indent_space);
        new_for->setBody(llvm_pointer_cast<Block>(body->clone()));
        return new_for;
    }
};

class AVNBlock : public Statement {
protected:
    vector<string> avn_var_decl;
    vector<string> avn_var_alloc;
    shared_ptr<Block> outer_block;

public:
    AVNBlock(shared_ptr<ForStmt> for_stmt) : Statement(STMT_AVN_BLOCK) {
        outer_block = std::make_shared<Block>(nullptr);
        outer_block->push_back(for_stmt);
    }

    void push_front(shared_ptr<Statement> for_stmt) {
        auto &body = outer_block->getBody();
        body.insert(body.begin(), for_stmt);
    }

    void addAVNVarDecl(const string &var) { avn_var_decl.push_back(var); }

    void addAVNVarAlloc(const string &var) { avn_var_alloc.push_back(var); }

    shared_ptr<Block> getBlock() { return outer_block; }

    shared_ptr<ForStmt> getForStmt() {
        return llvm_pointer_cast<ForStmt>(outer_block->getBody().front());
    }

    const vector<string> &getAVNVarDecl() const { return avn_var_decl; }

    vector<string> &getAVNVarDecl() { return avn_var_decl; }

    const vector<string> &getAVNVarAlloc() const { return avn_var_alloc; }

    vector<string> &getAVNVarAlloc() { return avn_var_alloc; }

    static bool classof(const Statement *stmt) { return stmt->get_kind() == STMT_AVN_BLOCK; }

    virtual string to_string() const override {
        return outer_block->to_string();
    }

    virtual shared_ptr<Statement> clone() const override {
        auto new_avn_block = std::make_shared<AVNBlock>(llvm_pointer_cast<ForStmt>(outer_block->getBody().front()));
        new_avn_block->avn_var_decl = avn_var_decl;
        new_avn_block->avn_var_alloc = avn_var_alloc;
        return new_avn_block;
    }

    shared_ptr<ForStmt> getInnermostFor() {
        shared_ptr<ForStmt> cur_for = llvm_pointer_cast<ForStmt>(outer_block->getBody().back());
        if (cur_for == nullptr) {
            return nullptr;
        }

        while (auto next = llvm_pointer_cast<ForStmt>(cur_for->getBody()->getBody().back())) {
            cur_for = next;
        }
        return cur_for;
    }

    // void setDepth(size_t depth) {
    //     for (auto &stmt : outer_block->getBody()) {
    //         if (auto for_stmt = llvm_pointer_cast<ForStmt>(stmt)) {
    //             for_stmt->setDepth(depth);
    //         }
    //     }
    // }
    // void setIndentSpace(size_t indent) {
    //     for (auto &stmt : outer_block->getBody()) {
    //         if (auto for_stmt = llvm_pointer_cast<ForStmt>(stmt)) {
    //             for_stmt->setIndentSpace(indent);
    //         }
    //     }
    // }
};

// class ArrayIndex {
//   protected:
//     vector<int> index_coefficient;
//     int constant;
//     shared_ptr<ForStmt> parent_loop;

//   public:
//     ArrayIndex(const vector<int> &index_coefficient, int constant, shared_ptr<ForStmt> parent_loop)
//         : index_coefficient(index_coefficient), constant(constant), parent_loop(parent_loop) {}
// };

// class ArrayRef : public Expression {
//   protected:
//     shared_ptr<DeclRef> array;
//     vector<shared_ptr<ArrayIndex>> indices;

//   public:
//     ArrayRef(shared_ptr<DeclRef> array, ) : Expression(EXPR_ARRAY_REF), array(array) {}
// };

// a * i + b
class Subscript {
public:
    enum ExprForm {
        aipb = 1,                // a * i + b
        const_num = 10,          // like 1, 2, etc.
        nonloop_index_var = 100, // like N, kte, etc.
        invalid = 1000,          // i / 2
    };

private:
    int a = 1;                       // coefficient of the loop variable i
    shared_ptr<DeclRef> i = nullptr; // loop variable or loop-independent variable
    int b = 0;                       // constant term
    ExprForm expr_form;

public:
    Subscript(shared_ptr<ir::Expression> idx_expr, const std::set<string> &loop_vars);
    Subscript(ExprForm form, const int &a, shared_ptr<DeclRef> i, const int &b) : a(a), i(i), b(b), expr_form(form) {}
    Subscript(const Subscript &other) : a(other.a), i(other.i), b(other.b), expr_form(other.expr_form) {}

    Subscript is_aipb_form(shared_ptr<ir::Expression> expr, const std::set<string> &loop_vars);
    inline bool isValid() const { return expr_form != ExprForm::invalid; }
    inline bool isLoopIndependent() const { return expr_form == ExprForm::const_num || expr_form == ExprForm::nonloop_index_var; }
    inline bool isConstNum() const { return expr_form == ExprForm::const_num; }
    inline bool isLoopIndependentVar() const { return expr_form == ExprForm::nonloop_index_var; }
    inline bool isAipb() const { return expr_form == ExprForm::aipb; }
    inline const ExprForm &getExprForm() const { return expr_form; }
    inline int getA() const { return a; }
    inline const shared_ptr<DeclRef> &getI() const { return i; }
    inline shared_ptr<DeclRef> getI() { return i; }
    inline int getB() const { return b; }
    inline void setA(int a) { this->a = a; }
    inline void setI(shared_ptr<DeclRef> i) { this->i = i; }
    inline void setB(int b) { this->b = b; }
    Subscript &operator=(const Subscript &other) {
        a = other.a;
        i = other.i;
        b = other.b;
        expr_form = other.expr_form;
        return *this;
    }
    shared_ptr<Expression> generateExpr() const {
        if (expr_form == ExprForm::aipb || expr_form == ExprForm::nonloop_index_var) {
            if (a == 1 && b == 0) {
                return i;
            } else if (a == 1) {
                if (b < 0)
                    return std::make_shared<SubOp>(i, std::make_shared<Number>(-b));
                else
                    return std::make_shared<AddOp>(i, std::make_shared<Number>(b));
            } else if (b == 0) {
                return std::make_shared<MulOp>(std::make_shared<Number>(a), i);
            } else {
                if (b < 0)
                    return std::make_shared<SubOp>(
                        std::make_shared<MulOp>(std::make_shared<Number>(a), i),
                        std::make_shared<Number>(-b));
                else
                    return std::make_shared<AddOp>(
                        std::make_shared<MulOp>(std::make_shared<Number>(a), i),
                        std::make_shared<Number>(b));
            }
        } else if (expr_form == ExprForm::const_num) {
            return std::make_shared<Number>(b);
        } else {
            return nullptr;
        }
    }
    string to_string() const {
        auto expr = this->generateExpr();
        if (expr)
            return expr->to_string();
        else
            return "invalid";
    }
};

class ArrayRef : public Expression {
protected:
    shared_ptr<DeclRef> array;
    /**
     * @brief The front element is the innermost index. e.g. X[j][i+1] -> indices = {i+1, j}.
     */
    vector<shared_ptr<Expression>> indices;
    vector<Subscript> norm_idx;
    std::map<string, boost::rational<int>> firstIdxOffset;
    ForStmt *parent_loop;
    size_t rpi = 0;

    // Called in constructor to set norm_idx.
    void computeNormIdx();
    // Called in constructor after computeNormIdx() to set rpi.
    void computeRPI();

public:
    /**
     * @param final is True if the array reference is no longer used by extraction, which is the situation
     * when we generate the final code and the array's subscripts may have modulo operator.
     */
    ArrayRef(shared_ptr<DeclRef> array, const vector<shared_ptr<Expression>> &indices, ForStmt *parent_loop, bool final = false)
        : Expression(EXPR_ARRAY_REF), array(array), indices(indices), parent_loop(parent_loop) {
        if (!final) {
            computeNormIdx();
            computeRPI();
        }
    }
    ArrayRef(const string &array_name, const vector<shared_ptr<Expression>> &indices, ForStmt *parent_loop, bool final = false)
        : ArrayRef(std::make_shared<DeclRef>(array_name), indices, parent_loop, final) {}
    ArrayRef(const string &array_name, const vector<Subscript> &subscripts, ForStmt *parent_loop);
    /**
     * @deprecated Use ArrayRef(const string &array_name, const vector<Subscript> &subscripts, ForStmt *parent_loop) instead.
     */
    ArrayRef(const string &array_name, const vector<std::pair<string, int>> &indices, ForStmt *parent_loop);

    const vector<shared_ptr<Expression>> &getIndices() const { return indices; }

    shared_ptr<DeclRef> getArrayName() const { return array; }

    /**
     * @deprecated Use getSubscripts() instead.
     */
    vector<std::pair<string, int>> getOrderedDisp() const;

    /**
     * @deprecated Use getSubscripts() instead.
     */
    std::map<string, int> getDisp() const;

    /**
     * @brief Set the parent loop of the array reference and recompute the array information if recompute_arr_info is true.
     */
    void setParentLoop(ForStmt *parent_loop, bool recompute_arr_info = true) {
        this->parent_loop = parent_loop;
        if (recompute_arr_info)
            recomputeArrayInfo();
    }

    void recomputeArrayInfo() {
        computeNormIdx();
        computeRPI();
    }

    const vector<Subscript> &getSubscripts() const {
        return norm_idx;
    }

    size_t getRPI() const { return rpi; }

    const std::map<string, boost::rational<int>> &getFirstIdxOffset() const { return firstIdxOffset; }

    ForStmt *getParentLoop() const { return parent_loop; }

    vector<shared_ptr<DeclRef>> getLoopVars() const {
        if (!parent_loop)
            return vector<shared_ptr<DeclRef>>();
        return parent_loop->getLoopVars();
    }

    static bool classof(const Expression *expr) {
        return expr->get_kind() >= EXPR_ARRAY_REF && expr->get_kind() < EXPR_ARRAY_REF_LAST;
    }
    virtual string to_string() const override;

    bool operator==(const ArrayRef &other) const {
        return array->to_string() == other.array->to_string();
    }

    bool operator<(const ArrayRef &other) const {
        return array->to_string() < other.array->to_string();
    }

    virtual shared_ptr<Expression> clone() const override {
        vector<shared_ptr<Expression>> new_indices;
        for (const auto &idx : indices) {
            new_indices.push_back(idx->clone());
        }
        // To avoid wrongly recompute the array information,
        // we set final to true and copy the norm_idx and rpi of this.
        auto res = std::make_shared<ArrayRef>(array, new_indices, parent_loop, true);
        res->norm_idx = norm_idx;
        res->rpi = rpi;
        res->firstIdxOffset = firstIdxOffset;
        return res;
    }

    bool isValid() {
        return std::all_of(norm_idx.begin(), norm_idx.end(), [](const Subscript &idx) { return idx.isValid(); });
    }
};

// class InterVar : public ir::ArrayRef {
// private:
//     shared_ptr<ir::Expression> expr;
//     std::map<string, int> upper_bounds;
//     std::map<string, int> lower_bounds;

// public:
//     InterVar(shared_ptr<ir::DeclRef> array, const vector<shared_ptr<ir::Expression>> &indices, ir::ForStmt *parent_loop,
//              shared_ptr<ir::Expression> expr, const std::map<string, int> &upper_bounds, const std::map<string, int> &lower_bounds)
//         : ir::ArrayRef(array, indices, parent_loop), expr(expr), upper_bounds(upper_bounds), lower_bounds(lower_bounds) {}

//     shared_ptr<ir::Expression> getExpr() const { return expr; }

//     static bool classof(const ir::Expression *expr) {
//         return expr->get_kind() == EXPR_INTER_VAR;
//     }
// };

/**
 * @brief Recursively parse an expression from clang::Expr to ir::Expression. If failed, throw an exception.
 * @param expr The clang::Expr to be parsed.
 * @param parent_loop The parent loop of the expression. Default to nullptr. Only used for ArrayRef.
 */
shared_ptr<Expression> parseExpression(const clang::Expr *expr, ForStmt *parent_loop);

// shared_ptr<Expression> buildExpression(std::pair<string, int> idx);
shared_ptr<ir::Expression> buildExpression(shared_ptr<ir::Expression> base_expr, int shift);

/**
 * @brief Generate a string representation of the expression with parentheses.
 * @param less_priority Control the parentheses conciseness. less_equal will consider the most parentheses, while less will ignore the parentheses of the same priority.
 */
inline string generateParenthesesStr(const Expression *expr, const Operator *parent_op,
                                     std::function<bool(int, int)> less_priority = std::less_equal<int>());

shared_ptr<DeclRef> searchDeclRef(shared_ptr<Expression> expr);

int exprEval(shared_ptr<ir::Expression> expr);

shared_ptr<ir::BinaryOp> buildBinaryOp(string op, shared_ptr<ir::Expression> l_expr, shared_ptr<ir::Expression> r_expr);

shared_ptr<ir::UnaryOp> buildUnaryOp(string op, shared_ptr<ir::Expression> expr);

template <typename T>
void for_each_expr(shared_ptr<ir::Expression> expr, std::function<void(shared_ptr<T>)> func) {
    if (auto ptr = llvm_pointer_cast<T>(expr)) {
        func(ptr);
    }
    if (auto bop = llvm_pointer_cast<ir::BinaryOp>(expr)) {
        for_each_expr<T>(bop->getLHS(), func);
        for_each_expr<T>(bop->getRHS(), func);
    } else if (auto uop = llvm_pointer_cast<ir::UnaryOp>(expr)) {
        for_each_expr<T>(uop->getOperand(), func);
    }
}

} // namespace ir

#endif