#ifndef ALIGNMENT_UTILS_H
#define ALIGNMENT_UTILS_H

#include <vector>
#include <stack>
#include <string>
#include <limits>

// 修改前向声明 - 只声明结构不使用实际类型
struct loopNode;
class ElementPtr;

void insertIterationsAndFixStack(
        std::vector<ElementPtr>& iterations,
        std::stack<ElementPtr>& stack,
        ElementPtr& curr,
        ElementPtr& parent,
        std::vector<ElementPtr>& functionalTraces);

void insertIterations(
        std::vector<ElementPtr>& iterations,
        ElementPtr& parent,
        std::vector<ElementPtr>& functionalTraces);

void levelUpStack(
        std::stack<ElementPtr>& stack,
        ElementPtr& curr,
        ElementPtr& parent);

void fixIterations(
        std::vector<ElementPtr>& functionalTraces,
        ElementPtr newChild);

bool isLoopEntry(
        std::string bbname,
        ElementPtr parent,
        struct loopNode *currloop);

void print(
        std::vector<ElementPtr>& functionalTraces,
        unsigned depth);

void printsurface(
        std::vector<ElementPtr>& functionalTraces);

void printSurfaceFunc(
        std::vector<ElementPtr>& functionalTraces,
        unsigned depth,
        std::string funcName = "");

#endif // ALIGNMENT_UTILS_H
