#ifndef EXTRACTOR_H
#define EXTRACTOR_H
#include "IR/ExprTree.h"

namespace extract {

using Forest = vector<shared_ptr<ir::ExprTree>>;
using TreeNodeVec = vector<shared_ptr<ir::ExprTreeNode>>;

void extract(shared_ptr<ir::AVNBlock> avn_block, int avn_cnt);

} // namespace extract

#endif
