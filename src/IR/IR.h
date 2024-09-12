#ifndef IR_H
#define IR_H
#include "clang/ASTMatchers/ASTMatchers.h"
#include "common.h"
#include <variant>

namespace IR {

using std::shared_ptr;
using std::string;
using std::vector;

enum PRIORITIES {
    PRIORITY_ASSIGN = 0,
    PRIORITY_ADD = 1,
    PRIORITY_MUL = 2,
    PRIORITY_UNARY = 3,
};

class Statement {
  public:
    enum StmtKind {
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
        EXPR_ARRAY_REF
    };

  private:
    const ExprKind kind;

  public:
    ExprKind get_kind() const { return kind; }
    Expression(ExprKind kind) : kind(kind) {}

    virtual ~Expression() {}
    virtual string to_string() const = 0;
};

template <typename To, typename From>
inline std::shared_ptr<To> llvm_pointer_cast(const std::shared_ptr<From> &e) {
    if (llvm::isa<To>(e.get())) {
        return std::static_pointer_cast<To>(e);
    } else {
        return nullptr;
    }
}

class Operator : public Expression {
  public:
    Operator(ExprKind kind) : Expression(kind) {}

    virtual ~Operator() {}
    virtual string to_string() const = 0;
    virtual PRIORITIES get_priority() const = 0;

    static bool classof(const Expression *expr) {
        return expr->get_kind() >= EXPR_OPERATOR && expr->get_kind() < EXPR_OPERATOR_LAST;
    }
};

class UnaryOp : public Operator {
  protected:
    const string op;
    shared_ptr<Expression> expr;

  public:
    UnaryOp(ExprKind kind, const string &op, shared_ptr<Expression> expr)
        : Operator(kind), op(op), expr(expr) {}
    virtual PRIORITIES get_priority() const override { return PRIORITY_UNARY; }
    virtual string to_string() const override;

    virtual shared_ptr<Expression> getOperand() const { return expr; }

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
};

class SinOp : public UnaryOp {
  public:
    SinOp(shared_ptr<Expression> expr) : UnaryOp(EXPR_SINOP, "sin", expr) {}
    virtual string to_string() const override;

    static bool classof(const Expression *expr) {
        return expr->get_kind() == EXPR_SINOP;
    }
};

class CosOp : public UnaryOp {
  public:
    CosOp(shared_ptr<Expression> expr) : UnaryOp(EXPR_COSOP, "cos", expr) {}
    virtual string to_string() const override;

    static bool classof(const Expression *expr) {
        return expr->get_kind() == EXPR_COSOP;
    }
};

class BinaryOp : public Operator {
  protected:
    const string op;
    shared_ptr<Expression> lhs, rhs;

  public:
    BinaryOp(ExprKind kind, const string &op, shared_ptr<Expression> lhs, shared_ptr<Expression> rhs)
        : Operator(kind), op(op), lhs(lhs), rhs(rhs) {}

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
};

class SubOp : public BinaryOp {
  public:
    SubOp() : BinaryOp(EXPR_SUBOP, "-", nullptr, nullptr) {}
    SubOp(shared_ptr<Expression> lhs, shared_ptr<Expression> rhs) : BinaryOp(EXPR_SUBOP, "-", lhs, rhs) {}

    virtual PRIORITIES get_priority() const override { return PRIORITY_ADD; }

    static bool classof(const Expression *expr) {
        return expr->get_kind() == EXPR_SUBOP;
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
};

class DivOp : public BinaryOp {
  public:
    DivOp() : BinaryOp(EXPR_DIVOP, "/", nullptr, nullptr) {}
    DivOp(shared_ptr<Expression> lhs, shared_ptr<Expression> rhs) : BinaryOp(EXPR_DIVOP, "/", lhs, rhs) {}

    virtual PRIORITIES get_priority() const override { return PRIORITY_MUL; }

    static bool classof(const Expression *expr) {
        return expr->get_kind() == EXPR_DIVOP;
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

    virtual string to_string() const override {
        if (std::holds_alternative<LL>(value))
            return std::to_string(std::get<LL>(value));
        else
            return std::to_string(std::get<double>(value));
    }

    static bool classof(const Expression *expr) {
        return expr->get_kind() == EXPR_NUMBER;
    }
};

class Assignment : public Statement {
  protected:
    shared_ptr<DeclRef> lhs;
    shared_ptr<Expression> rhs;

  public:
    Assignment(shared_ptr<DeclRef> lhs, shared_ptr<Expression> rhs) : Statement(STMT_ASSIGN), lhs(lhs), rhs(rhs) {}

    virtual string to_string() const override { return lhs->to_string() + " = " + rhs->to_string(); }
    virtual shared_ptr<DeclRef> getLHS() const { return lhs; }
    virtual shared_ptr<Expression> getRHS() const { return rhs; }
    virtual void setLHS(shared_ptr<DeclRef> lhs) { this->lhs = lhs; }
    virtual void setRHS(shared_ptr<Expression> rhs) { this->rhs = rhs; }

    static bool classof(const Statement *stmt) { return stmt->get_kind() == STMT_ASSIGN; }
};

class ForStmt : public Statement {
    // Parse and set the loop variable, begin, and end from the for statement.
    void buildBounds(const clang::ForStmt *for_stmt);
    void processInnerStmt(const clang::Stmt *stmt);

  protected:
    shared_ptr<DeclRef> loop_var;
    shared_ptr<Expression> begin, end; // begin <= loop_var <= end
    vector<shared_ptr<Statement>> body;
    vector<shared_ptr<DeclRef>> index_vars;
    unsigned depth;

  public:
    /**
     * @brief Construct a ForStmt object from a clang::ForStmt object.
     * @warning Parent_for cannot be shared_ptr<ForStmt> because of the constructor uses this.
     * And shared_from_this() can only be called after the object is constructed.
     */
    ForStmt(const clang::ForStmt *for_stmt, const ForStmt *parent_for = nullptr);
    static bool classof(const Statement *stmt) { return stmt->get_kind() == STMT_FOR; }
    virtual string to_string() const override;
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
     * @brief The front element is the innermost index. e.g. X[j][i] -> indices = {i, j}.
     */
    vector<shared_ptr<Expression>> indices;

  public:
    ArrayRef(shared_ptr<DeclRef> array, const vector<shared_ptr<Expression>> &indices);

    const vector<shared_ptr<Expression>> &getIndices() const { return indices; }

    shared_ptr<DeclRef> getArray() const { return array; }

    static bool classof(const Expression *expr) {
        return expr->get_kind() == EXPR_ARRAY_REF;
    }
    virtual string to_string() const override;
};

shared_ptr<Expression> parseExpression(const clang::Expr *expr);

inline string generateParenthesesStr(const Expression *expr, const Operator *parent_op);

} // namespace IR

#endif