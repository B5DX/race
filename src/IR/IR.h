#ifndef IR_H
#define IR_H
#include <common.h>

// Restrain the scope of the using directive to the current file.
namespace {
using std::shared_ptr;
using std::string;
using std::vector;

enum PRIORITIES {
    PRIORITY_ASSIGN = 0,
    PRIORITY_ADD = 1,
    PRIORITY_MUL = 2,
    PRIORITY_UNARY = 3,
};
} // namespace

namespace IR {

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

    static bool classof(const Expression *expr) {
        return expr->get_kind() >= EXPR_BINARY && expr->get_kind() < EXPR_BINARY_LAST;
    }
};

class AddOp : public BinaryOp {
  public:
    AddOp(shared_ptr<Expression> lhs, shared_ptr<Expression> rhs) : BinaryOp(EXPR_ADDOP, "+", lhs, rhs) {}

    virtual PRIORITIES get_priority() const override { return PRIORITY_ADD; }

    static bool classof(const Expression *expr) {
        return expr->get_kind() == EXPR_ADDOP;
    }
};

class SubOp : public BinaryOp {
  public:
    SubOp(shared_ptr<Expression> lhs, shared_ptr<Expression> rhs) : BinaryOp(EXPR_SUBOP, "-", lhs, rhs) {}

    virtual PRIORITIES get_priority() const override { return PRIORITY_ADD; }

    static bool classof(const Expression *expr) {
        return expr->get_kind() == EXPR_SUBOP;
    }
};

class MulOp : public BinaryOp {
  public:
    MulOp(shared_ptr<Expression> lhs, shared_ptr<Expression> rhs) : BinaryOp(EXPR_MULOP, "*", lhs, rhs) {}

    virtual PRIORITIES get_priority() const override { return PRIORITY_MUL; }

    static bool classof(const Expression *expr) {
        return expr->get_kind() == EXPR_MULOP;
    }
};

class DivOp : public BinaryOp {
  public:
    DivOp(shared_ptr<Expression> lhs, shared_ptr<Expression> rhs) : BinaryOp(EXPR_DIVOP, "/", lhs, rhs) {}

    virtual PRIORITIES get_priority() const override { return PRIORITY_MUL; }

    static bool classof(const Expression *expr) {
        return expr->get_kind() == EXPR_DIVOP;
    }
};

class AssignOp : public BinaryOp {
  public:
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

class ForStmt : public Statement {
  protected:
    shared_ptr<DeclRef> loop_var;
    shared_ptr<Expression> begin, end;
    vector<shared_ptr<Statement>> body;
    vector<shared_ptr<DeclRef>> index_vars;

  public:
    ForStmt(shared_ptr<DeclRef> loop_var, shared_ptr<Expression> begin, shared_ptr<Expression> end,
            shared_ptr<ForStmt> parent_for) : Statement(STMT_FOR), loop_var(loop_var), begin(begin), end(end) {
        if (parent_for)
            index_vars = parent_for->index_vars;
        index_vars.push_back(loop_var);
    }
};

class ArrayIndex {
  protected:
    vector<int> index_coefficient;
    int constant;
    shared_ptr<ForStmt> parent_loop;

  public:
};

class ArrayRef : public Expression {
  protected:
    shared_ptr<DeclRef> array;
    vector<shared_ptr<ArrayIndex>> indices;

  public:
    ArrayRef(shared_ptr<DeclRef> array) : Expression(EXPR_ARRAY_REF), array(array) {}
};

} // namespace IR

#endif