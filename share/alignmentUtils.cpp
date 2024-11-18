
#include "alignmentUtils.h"

using namespace std;

void insertIterationsAndFixStack(
        vector<shared_ptr<element>> &iterations,
        stack<shared_ptr<element>> &stack,
        shared_ptr<element> &curr,
        shared_ptr<element> &parent,
        vector<shared_ptr<element>> &functionalTraces) {
    if(!stack.empty()) {
        stack.top()->funcs.back().insert(
                stack.top()->funcs.back().end(),
                iterations.begin(),
                iterations.end());
        curr = parent;
        parent = stack.top();
        stack.pop();
    } else {
        functionalTraces.insert(
                functionalTraces.end(),
                iterations.begin(),
                iterations.end());
        curr = parent;
        parent = nullptr;
    }
    return;
}

void insertIterations(
        vector<shared_ptr<element>> &iterations,
        shared_ptr<element> &parent,
        vector<shared_ptr<element>> &functionalTraces) {
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
        stack<shared_ptr<element>> &stack,
        shared_ptr<element> &curr,
        shared_ptr<element> &parent) {
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
        vector<shared_ptr<element>>& functionalTraces,
        shared_ptr<element> newChild) {
    if(functionalTraces.size() == 0) {
        return;
    }
    if(functionalTraces.back()->bb() == newChild->bb() 
            && functionalTraces.back()->isLoop == false
            && newChild->isLoop == false) {
        if(functionalTraces.back()->loopIndex == -1) {
            functionalTraces.back()->loopIndex = 1;
            newChild->loopIndex = 2;
        } else {
            newChild->loopIndex = functionalTraces.back()->loopIndex + 1;
        }
    } 
}
