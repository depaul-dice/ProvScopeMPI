
#include "alignmentUtils.h"

using namespace std;

void insertIterationsAndFixStack(
        vector<shared_ptr<element>> &iterations,
        stack<shared_ptr<element>> &stack,
        shared_ptr<element> &curr,
        shared_ptr<element> &parent,
        vector<shared_ptr<element>> &functionalTraces) {
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

/*
 * checks if the bbname is the entry of the loop with parent's node
 */
bool isLoopEntry(
        string bbname, 
        shared_ptr<element> parent, 
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
        vector<shared_ptr<element>>& functionalTraces, 
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
        vector<shared_ptr<element>>& functionalTraces) {
    for(auto& eptr : functionalTraces) {
        fprintf(stderr, "%s\n", eptr->bb().c_str());
    }
}

void printSurfaceFunc(
        vector<shared_ptr<element>>& functionalTraces,
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
