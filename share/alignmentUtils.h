#ifndef ALIGNMENT_UTILS_H
#define ALIGNMENT_UTILS_H

#include <vector>
#include <stack>
#include <string>
#include <limits>
#include <memory>

// 前向声明
struct element;
struct loopNode;

// 确保使用原始代码中的ElementPtr类型
// 而不是我们之前添加的ElementSharedPtr
class ElementPtr;  // 或者这里需要完整的声明，取决于原始定义

// 修改所有函数声明使用ElementPtr
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
        loopNode *currloop);

void print(
        std::vector<ElementPtr>& functionalTraces,
        unsigned depth);

void printsurface(
        std::vector<ElementPtr>& functionalTraces);

void printSurfaceFunc(
        std::vector<ElementPtr>& functionalTraces,
        unsigned depth,
        std::string funcName);

#endif // ALIGNMENT_UTILS_H
