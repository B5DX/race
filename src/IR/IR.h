#ifndef IR_H
#define IR_H
#include "common.h"
#include "llvm/Support/Casting.h"
#include <functional>
#include <map>
#include <set>
#include <variant>

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
};

class Statement {
public:
    enum StmtKind {
        STMT_BLOCK,
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
};

class Expression {
public:
    enum ExprKind {
        EXPR_OPERATOR,
        EXPR_UNARY,
        EXPR_NEGOP,
        EXPR_SINOP,
        EXPR_COSOP,
        EXPR_UNARY_LAST,
        EXPR_BINARY,
        EXPR_ADDOP,
        EXPR_SUBOP,
        EXPR_MULOP,
        EXPR_DIVOP,
        EXPR_ASSIGN,
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

    virtual shared_ptr<Expression> clone() const override {
        return std::make_shared<NegOp>(expr->clone());
    }
};

class SinOp : public UnaryOp {
public:
    SinOp(shared_ptr<Expression> expr) : UnaryOp(EXPR_SINOP, "sin", expr) {}
    virtual string to_string() const override;

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
    virtual string to_string() const override;

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
        if (std::holds_alternative<LL>(value))
            return std::to_string(std::get<LL>(value));
        else
            return std::to_string(std::get<double>(value));
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
    shared_ptr<Expression> lhs;
    shared_ptr<Expression> rhs;

public:
    Assignment(shared_ptr<Expression> lhs, shared_ptr<Expression> rhs) : Statement(STMT_ASSIGN), lhs(lhs), rhs(rhs) {}

    virtual string to_string() const override { return lhs->to_string() + " = " + rhs->to_string(); }
    virtual shared_ptr<Expression> getLHS() const { return lhs; }
    virtual shared_ptr<Expression> getRHS() const { return rhs; }
    virtual void setLHS(shared_ptr<Expression> lhs) { this->lhs = lhs; }
    virtual void setRHS(shared_ptr<Expression> rhs) { this->rhs = rhs; }

    static bool classof(const Statement *stmt) { return stmt->get_kind() == STMT_ASSIGN; }
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
};

class ForStmt : public Statement {
    // Parse and set the loop variable, begin, and end from the for statement.
    void buildBounds(const clang::ForStmt *for_stmt);
    void processInnerStmt(const clang::Stmt *stmt);

protected:
    shared_ptr<DeclRef> loop_var;
    shared_ptr<Expression> begin, end; // begin <= loop_var <= end
    Block body;
    /**
     * @brief The front element is the innermost index.
     * e.g. for(j...) {for(i...) {}} -> index_vars = {i, j}.
     */
    vector<shared_ptr<DeclRef>> index_vars;
    ForStmt *parent_for;
    unsigned depth;

public:
    /**
     * @brief Construct a ForStmt object from a clang::ForStmt object.
     * @warning Parent_for cannot be shared_ptr<ForStmt> because of the constructor uses this.
     * And shared_from_this() can only be called after the object is constructed.
     */
    ForStmt(const clang::ForStmt *for_stmt, ForStmt *parent_for = nullptr);
    ForStmt(string loop_var, shared_ptr<Expression> begin, shared_ptr<Expression> end, ForStmt *parent_for)
        : Statement(STMT_FOR), begin(begin), end(end), body(this), parent_for(const_cast<ForStmt *>(parent_for)), depth(parent_for ? parent_for->depth + 1 : 0) {
        this->loop_var = std::make_shared<DeclRef>(loop_var);
        index_vars.push_back(this->loop_var);
        if (parent_for) {
            const auto &parent_index_vars = parent_for->index_vars;
            index_vars.insert(index_vars.end(), parent_index_vars.begin(), parent_index_vars.end());
        }
    }
    static bool classof(const Statement *stmt) { return stmt->get_kind() == STMT_FOR; }
    virtual string to_string() const override;

    // TODO: Block may be defined as a class named AssignBlock inherited from Statement, which can contain the loop_vars information for ArrayRef's index.
    vector<Block> getInnermostBlocks();

    const vector<shared_ptr<DeclRef>> &getLoopVars() const {
        return index_vars;
    }

    void setParentFor(ForStmt *parent_for) { 
        this->index_vars = {this->loop_var};
        if (parent_for) {
            const auto &parent_index_vars = parent_for->index_vars;
            index_vars.insert(index_vars.end(), parent_index_vars.begin(), parent_index_vars.end());
            this->depth = parent_for->depth + 1;
        }
        this->parent_for = parent_for; 
    }
    ForStmt *getParentFor() { return parent_for; }
    shared_ptr<DeclRef> getLoopVar() const { return loop_var; }
    shared_ptr<Expression> getBegin() const { return begin; }
    shared_ptr<Expression> getEnd() const { return end; }
    const Block &getBody() const { return body; }
    Block &getBody() { return body; }
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

class ArrayRef : public Expression {
protected:
    shared_ptr<DeclRef> array;
    /**
     * @brief The front element is the innermost index. e.g. X[j][i+1] -> indices = {i+1, j}.
     */
    vector<shared_ptr<Expression>> indices;
    ForStmt *parent_loop;

public:
    ArrayRef(shared_ptr<DeclRef> array, const vector<shared_ptr<Expression>> &indices, ForStmt *parent_loop)
        : Expression(EXPR_ARRAY_REF), array(array), indices(indices), parent_loop(parent_loop) {};
    ArrayRef(const string &array_name, const vector<std::pair<string, int>> &indices, ForStmt *parent_loop);

    const vector<shared_ptr<Expression>> &getIndices() const { return indices; }

    shared_ptr<DeclRef> getArrayName() const { return array; }

    vector<std::pair<string, int>> getOrderedDisp() const;
    
    std::map<string, int> getDisp() const;

    void setParentLoop(ForStmt *parent_loop) { this->parent_loop = parent_loop; }

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
        return std::make_shared<ArrayRef>(array, new_indices, parent_loop);
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
    } else if (auto bop = llvm_pointer_cast<ir::BinaryOp>(expr)) {
        for_each_expr<T>(bop->getLHS(), func);
        for_each_expr<T>(bop->getRHS(), func);
    } else if (auto uop = llvm_pointer_cast<ir::UnaryOp>(expr)) {
        for_each_expr<T>(uop->getOperand(), func);
    }
}

} // namespace ir

#endif