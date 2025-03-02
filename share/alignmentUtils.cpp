#include "alignment.h"
#include "alignmentUtils.h"

using namespace std;

void insertIterationsAndFixStack(
        vector<ElementPtr> &iterations,
        stack<ElementPtr> &stack,
        ElementPtr &curr,
        ElementPtr &parent,
        vector<ElementPtr> &functionalTraces) {
    curr = parent;
    if(!stack.empty()) {
        stack.top()->funcs.back().insert(
                stack.top()->funcs.back().end(),
                iterations.begin(),
                iterations.end());
        parent = stack.top();
        stack.pop();
    } else {
        functionalTraces.insert(
                functionalTraces.end(),
                iterations.begin(),
                iterations.end());
        parent = nullptr;
    }
    return;
}

void insertIterations(
        vector<ElementPtr> &iterations,
        ElementPtr &parent,
        vector<ElementPtr> &functionalTraces) {
    if(parent != nullptr) {
        parent->funcs.back().insert(
                parent->funcs.back().end(),
                iterations.begin(),
                iterations.end());
    } else {
        functionalTraces.insert(
                functionalTraces.end(),
                iterations.begin(),
                iterations.end());
    }
    return;
}

void levelUpStack(
        stack<ElementPtr> &stack,
        ElementPtr &curr,
        ElementPtr &parent) {
    curr = parent;
    if(!stack.empty()) {
        parent = stack.top();
        stack.pop();
    } else {
        parent = nullptr;
    }
    return;
}

void fixIterations(
        vector<ElementPtr>& functionalTraces,
        ElementPtr newChild) {
    // 严格检查输入参数
    if(functionalTraces.empty() || !newChild || !newChild.get()) {
        return;
    }

    // 预分配所需的容量
    if(functionalTraces.capacity() < functionalTraces.size() + 1) {
        functionalTraces.reserve(functionalTraces.size() * 2);
    }

    ElementPtr lastChild = functionalTraces.back();
    if(!lastChild || !lastChild.get()) {
        return; // 防止空指针访问
    }

    // 安全地构造字符串
    string lastChildBB;
    string newChildBB;

    try {
        if(lastChild->funcname.empty() || newChild->funcname.empty()) {
            return; // 函数名为空，不继续处理
        }

        // 手动构造BB字符串，避免调用可能不安全的bb()方法
        if(lastChild->isEntry) {
            lastChildBB = lastChild->funcname + ":entry:" + to_string(lastChild->id);
        } else if(lastChild->isExit) {
            lastChildBB = lastChild->funcname + ":exit:" + to_string(lastChild->id);
        } else {
            lastChildBB = lastChild->funcname + ":neither:" + to_string(lastChild->id);
        }

        if(newChild->isEntry) {
            newChildBB = newChild->funcname + ":entry:" + to_string(newChild->id);
        } else if(newChild->isExit) {
            newChildBB = newChild->funcname + ":exit:" + to_string(newChild->id);
        } else {
            newChildBB = newChild->funcname + ":neither:" + to_string(newChild->id);
        }
    } catch(...) {
        fprintf(stderr, "ERROR: Exception in fixIterations while constructing BB strings\n");
        return;
    }

    // 比较并设置循环索引
    if(lastChildBB == newChildBB
            && lastChild->isLoop == false
            && newChild->isLoop == false) {
        try {
            if(lastChild->loopIndex == -1) {
                lastChild->loopIndex = 1;
                newChild->loopIndex = 2;
            } else {
                newChild->loopIndex = lastChild->loopIndex + 1;
            }
        } catch(...) {
            fprintf(stderr, "ERROR: Exception in fixIterations while setting loop indices\n");
            return;
        }
    }
}

/*
 * checks if the bbname is the entry of the loop with parent's node
 */
bool isLoopEntry(
        string bbname,
        ElementPtr parent,
        loopNode *currloop) {
    string parentbb;
    if(parent == nullptr) return false;
    if(parent->isEntry) {
        MPI_EQUAL(parent->isExit, false);
        parentbb = parent->funcname + ":entry:" + to_string(parent->id);
    } else if(parent->isExit) {
        parentbb = parent->funcname + ":exit:" + to_string(parent->id);
    } else {
        parentbb = parent->funcname + ":neither:" + to_string(parent->id);
    }
    return parent->isLoop
        && parentbb == bbname
        && currloop->nodes.size() > 0
        && bbname == currloop->entry;
}

void print(
        vector<ElementPtr>& functionalTraces,
        unsigned depth) {
    for(auto& eptr : functionalTraces) {
        for(unsigned i = 0; i < depth; i++) {
            fprintf(stderr, "\t");
        }
        if(eptr->index != numeric_limits<unsigned long>::max()) {
            if(eptr->isLoop) {
                fprintf(stderr, "%s:%lu\n",
                        eptr->bb().c_str(), eptr->index);
            } else {
                fprintf(stderr, "%s:%lu\n",
                        eptr->bb().c_str(), eptr->index);
            }
        } else {
            fprintf(stderr, "%s\n", eptr->bb().c_str());
        }

        for(unsigned i = 0; i < eptr->funcs.size(); i++) {
            print(eptr->funcs[i], depth + 1);
        }
    }
}

void printsurface(
        vector<ElementPtr>& functionalTraces) {
    for(auto& eptr : functionalTraces) {
        fprintf(stderr, "%s\n", eptr->bb().c_str());
    }
}

void printSurfaceFunc(
        vector<ElementPtr>& functionalTraces,
        unsigned depth,
        string funcName) {
    for(auto& eptr : functionalTraces) {
        if(eptr->funcname == funcName
                || funcName == "") {
            for(unsigned i = 0; i < depth; i++) {
                fprintf(stderr, "\t");
            }
            fprintf(stderr, "%s\n", eptr->bb().c_str());
            for(unsigned i = 0; i < eptr->funcs.size(); i++) {
                printSurfaceFunc(eptr->funcs[i], depth + 1, eptr->funcname);
            }
        } else {
            break;
        }
    }
}
