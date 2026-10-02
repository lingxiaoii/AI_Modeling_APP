#include "tools/triangle_budget.h"

namespace pm::tools {
namespace {

TriangleBudget g_budget;

}  // namespace

TriangleBudget& triangle_budget() { return g_budget; }

}  // namespace pm::tools