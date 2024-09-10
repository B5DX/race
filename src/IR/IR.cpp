#include <IR/IR.h>

using std::shared_ptr;
using std::string;
using std::vector;
using std::dynamic_pointer_cast;

bool is_operator(shared_ptr<Expression> expr) {
    return dynamic_pointer_cast<BinaryOp>(expr) || dynamic_pointer_cast<UnaryOp>(expr);
}
