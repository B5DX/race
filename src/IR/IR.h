#ifndef IR_H
#define IR_H
#include <common.h>

// Restrain the scope of the using directive to the current file.
namespace {
    using std::shared_ptr;
    using std::string;
    using std::vector;
    using std::dynamic_pointer_cast;

    enum PRIORITIES {
        PRIORITY_ADD = 1,
        PRIORITY_MUL = 2,
        PRIORITY_UNARY = 3,
    };
}

class Statement {
public:
    virtual ~Statement() {}
    virtual string to_string() const = 0;
};

class Expression {
public:
    virtual ~Expression() {}
    virtual string to_string() const = 0;
};

class UnaryOp: public Expression {
protected:
    static const PRIORITIES priority = PRIORITY_UNARY;
    shared_ptr<Expression> expr;
    
public:
    UnaryOp(shared_ptr<Expression> expr): expr(expr) {}
};

class NegOp: public UnaryOp {
public:
    NegOp(shared_ptr<Expression> expr): UnaryOp(expr) {}

    virtual string to_string() const override;
};

class SinOp: public UnaryOp {
public:
    SinOp(shared_ptr<Expression> expr): UnaryOp(expr) {}
    
    virtual string to_string() const override;
};

class CosOp: public UnaryOp {
public:
    CosOp(shared_ptr<Expression> expr): UnaryOp(expr) {}

    virtual string to_string() const override;
};

class BinaryOp: public Expression {
protected:
    static const PRIORITIES priority;
    shared_ptr<Expression> lhs, rhs;

public:
    BinaryOp(shared_ptr<Expression> lhs, shared_ptr<Expression> rhs): lhs(lhs), rhs(rhs) {}
};

class AddOp: public BinaryOp {
public:
    AddOp(shared_ptr<Expression> lhs, shared_ptr<Expression> rhs): BinaryOp(lhs, rhs) {}
    
    virtual string to_string() const override;
};

class SubOp: public BinaryOp {
public:
    SubOp(shared_ptr<Expression> lhs, shared_ptr<Expression> rhs): BinaryOp(lhs, rhs) {}
};

class MulOp: public BinaryOp {
public:
    MulOp(shared_ptr<Expression> lhs, shared_ptr<Expression> rhs): BinaryOp(lhs, rhs) {}
};

class DivOp: public BinaryOp {
public:
    DivOp(shared_ptr<Expression> lhs, shared_ptr<Expression> rhs): BinaryOp(lhs, rhs) {}
};

class ForStmt: public Statement {
public:
    shared_ptr<Statement> init, cond, inc, body;
    ForStmt(shared_ptr<Statement> init, shared_ptr<Statement> cond, shared_ptr<Statement> inc, shared_ptr<Statement> body): init(init), cond(cond), inc(inc), body(body) {}
};

class DeclRef: public Expression {
public:
    string name;
    DeclRef(const string &name): name(name) {}
};

class ArrayRef: public Expression {
public:
    const char *name;
    Expression *index;
    ArrayRef(const char *name, Expression *index): name(name), index(index) {}
};



#endif