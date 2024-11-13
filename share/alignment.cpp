
#include "alignment.h"

using namespace std;

/*
 * recordTracesRaw should not be visible to mpireproduce.cpp
 */
vector<vector<string>> recordTracesRaw;
vector<shared_ptr<element>> recordTraces;

/*
 * this is necessary for bbprinter
 */
vector<vector<string>> replayTracesRaw; 
vector<shared_ptr<element>> replayTraces;

static  unordered_map<string, unordered_set<string>> divs;

/*
 * below stores points of divergences and dumps at MPI_Finalize
 */
unordered_set<string> pods;

const unordered_set<string> mpiCallsWithSendNodesAtLast = {
    "MPI_Recv",
    "MPI_Test",
    "MPI_Wait"
};

/*
 * this is simply the declaration of the function for a reference
 */
vector<shared_ptr<element>> __makeHierarchyLoop(
        vector<vector<string>>& traces, 
        unsigned long& index, 
        unordered_map<string, loopNode *>& loopTrees, 
        loopNode *currloop,
        int iterationCnt = 1);

element::element(
        bool isEntry, 
        bool isExit, 
        int id, 
        string& funcname, 
        bool isLoop) : 
    funcs(vector<vector<shared_ptr<element>>>()), 
    funcname(funcname), 
    index(numeric_limits<unsigned long>::max()), 
    id(id), 
    isEntry(isEntry), 
    isExit(isExit), 
    isLoop(isLoop) {
}

element::element(
        bool isEntry, 
        bool isExit, 
        int id, 
        string& funcname, 
        unsigned long index, 
        bool isLoop) :
    funcs(vector<vector<shared_ptr<element>>>()), 
    funcname(funcname), 
    index(index), 
    id(id), 
    isEntry(isEntry), 
    isExit(isExit), 
    isLoop(isLoop) {
}

/*
 * this is an initialization for a loop node
 */
element::element(
        int id, 
        string& funcname, 
        unsigned long index, 
        bool isLoop) :
    funcs(vector<vector<shared_ptr<element>>>()), 
    funcname(funcname), 
    index(index), 
    id(id), 
    isEntry(false), 
    isExit(false), 
    isLoop(isLoop) {
}

string element::bb() const {
    string str = funcname + ":";
    if(isEntry && isExit) {
        str += "both:" + to_string(id);
    } else if(isEntry) {
        str += "entry:" + to_string(id);
    } else if(isExit) {
        str += "exit:" + to_string(id);
    } else {
        str += "neither:" + to_string(id);
    }
    if(isLoop) {
        str += ":loop";
    }
    return str;
}

/*
 * this does not print whether the node is a loop node or not
 */
string element::content() const {
    string str = funcname + ":";
    if(isEntry && isExit) {
        str += "both:" + to_string(id);
    } else if(isEntry) {
        str += "entry:" + to_string(id);
    } else if(isExit) {
        str += "exit:" + to_string(id);
    } else {
        str += "neither:" + to_string(id);
    }
    return str;
}

bool element::operator ==(const element &e) const {
    return (
            isEntry == e.isEntry 
            && isExit == e.isExit 
            && id == e.id 
            && funcname == e.funcname 
            && isLoop == e.isLoop);
}

ostream& operator << (ostream& os, const element& e) {
    if(e.isLoop == true) {
        os << e.bb() <<  e.index;
    } else {
        os << e.bb() << ':' << e.index;
    }
    return os;
}

lastaligned::lastaligned() : 
    funcId(numeric_limits<unsigned long>::max()), 
    origIndex(numeric_limits<unsigned long>::max()), 
    repIndex(numeric_limits<unsigned long>::max()) {
}

lastaligned::lastaligned(
        unsigned long funcId, 
        unsigned long origIndex, 
        unsigned long repIndex) : 
    funcId(funcId), 
    origIndex(origIndex), 
    repIndex(repIndex) {
}

bool lastaligned::isSuccess() {
    return origIndex == numeric_limits<unsigned long>::max() 
        && repIndex == numeric_limits<unsigned long>::max();
}

ostream& operator<<(ostream& os, const lastaligned& l) {
    os << "funcId: " << l.funcId << 
        ", origIndex: " << l.origIndex << 
        ", repIndex: " << l.repIndex;
    return os;
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

vector<shared_ptr<element>> makeHierarchyMain(
        vector<vector<string>>& traces, unsigned long& index) {
    vector<shared_ptr<element>> functionalTraces;
    bool isEntry, 
         isExit;
    string funcname = "main", 
           bbname;
    while(index < traces.size() 
            && funcname != traces[index][0]) {
        index++;
    }

    if(index >= traces.size()) {
        DEBUG("index: %lu, traces.size(): %lu\n", 
                index, traces.size()); 
        return functionalTraces;
    }

    do {
        MPI_EQUAL(traces[index][0], funcname);
        MPI_ASSERT(traces[index].size() == 3 
                || traces[index].size() == 4);
        bbname = traces[index][0] + ":" + traces[index][1] + ":" + traces[index][2]; 
        isEntry = (traces[index][1] == "entry");
        isExit = (traces[index][1] == "exit");
        /* DEBUG0("bbname: %s\n", bbname.c_str()); */
        shared_ptr<element> eptr;
        if(traces[index].size() == 4) {
            eptr = make_shared<element>(
                    isEntry, 
                    isExit, 
                    stoi(traces[index][2]), 
                    traces[index][0], 
                    stoul(traces[index][3]));
        } else {
            eptr = make_shared<element>(
                    isEntry, 
                    isExit, 
                    stoi(traces[index][2]), 
                    traces[index][0]);
        }
        index++;
    
        while(isExit == false 
                && index < traces.size() 
                && traces[index][1] == "entry") {
            eptr->funcs.push_back(
                    makeHierarchy(traces, index));
        }
        fixIterations(functionalTraces, eptr);
        functionalTraces.push_back(eptr);
    } while(!isExit 
            && index < traces.size());

    return functionalTraces;
}

/*
 * returns the child loopNode
 */
static loopNode *isNewLoop(
        string bbname, 
        loopNode *ln) {
    MPI_ASSERT(ln != nullptr);
    /* DEBUG0("isNewLoop with bbname: %s, at ln of %s\n", bbname.c_str(), ln->entry.c_str()); */
    for(auto &c: ln->children) {
        if(c->nodes.find(bbname) != c->nodes.end()) {
            return c;
        }
    }
    return nullptr;
}

vector<shared_ptr<element>> makeHierarchyLoop(
        vector<vector<string>>& traces, 
        unsigned long& index, 
        unordered_map<string, loopNode *>& loopTrees)  {
    vector<shared_ptr<element>> functionalTraces;
    shared_ptr<element> eptr;
    bool isEntry = false, 
         isExit = false;
    string bbname, funcname = traces[index][0];
    loopNode *currloop = nullptr, 
             *child = nullptr;
    int iterationcnt = 1;

    // here we are getting the entry 
    MPI_EQUAL(traces[index][1], "entry");
    currloop = loopTrees[funcname];

    while (!isExit 
            && index < traces.size()) {
        MPI_ASSERT(index < traces.size());
        bbname = traces[index][0] 
            + ":" + traces[index][1] 
            + ":" + traces[index][2];
        MPI_EQUAL(traces[index][0], funcname);
        MPI_ASSERT(traces[index].size() == 3 
                || traces[index].size() == 4);
        isEntry = (traces[index][1] == "entry");
        isExit = (traces[index][1] == "exit");

        /* 
         * case 1: if we are in the inner loop, 
         * recursively call the function
         */
        if((child = isNewLoop(bbname, currloop)) != nullptr) {
            vector<shared_ptr<element>> loops = __makeHierarchyLoop(
                        traces, 
                        index, 
                        loopTrees, 
                        child);
            functionalTraces.insert(
                    functionalTraces.end(), 
                    loops.begin(), 
                    loops.end());
        } else {       
            if(traces[index].size() == 4) {
                eptr = make_shared<element>(
                        isEntry, 
                        isExit, 
                        stoi(traces[index][2]), 
                        traces[index][0], 
                        stoul(traces[index][3]));
            } else {
                eptr = make_shared<element>(
                        isEntry, 
                        isExit, 
                        stoi(traces[index][2]), 
                        traces[index][0]);
            }
            index++;
            while(!isExit 
                    && index < traces.size() 
                    && traces[index][1] == "entry") {

                eptr->funcs.push_back(
                        makeHierarchyLoop(
                            traces, 
                            index, 
                            loopTrees)); 
            }
            fixIterations(functionalTraces, eptr);
            functionalTraces.push_back(eptr);
        }
    } 

    return functionalTraces; 
}

vector<shared_ptr<element>> __makeHierarchyLoop(
        vector<vector<string>>& traces, 
        unsigned long& index, 
        unordered_map<string, loopNode *>& loopTrees, 
        loopNode *currloop,
        int iterationCnt) {
    vector<shared_ptr<element>> iterations, curriter;
    shared_ptr<element> eptr;
    bool isEntry = false, 
         isExit = false;
    string bbname, 
           funcname = traces[index][0];
    loopNode *child = nullptr;

    // here we are getting the entry 
    vector<string> entryinfo = parse(currloop->entry, ':');
    MPI_ASSERT(entryinfo.size() == 3);
    MPI_EQUAL(entryinfo[0], funcname);
    int entryindex = stoi(entryinfo[2]);

    while (!isExit && index < traces.size()) {
        MPI_ASSERT(index < traces.size());
        bbname = traces[index][0] + ":" + traces[index][1] + ":" + traces[index][2];
        MPI_EQUAL(traces[index][0], funcname);
        MPI_ASSERT(traces[index].size() == 3 
                || traces[index].size() == 4);
        isEntry = (traces[index][1] == "entry");
        isExit = (traces[index][1] == "exit");

        /* 
         * case 1: if we are in the inner loop, 
         * recursively call the function
         */
        if((child = isNewLoop(bbname, currloop)) != nullptr) {
            /* DEBUG0("printing bbname: %s\n", bbname.c_str()); */
            vector<shared_ptr<element>> loops = __makeHierarchyLoop(
                    traces, 
                    index, 
                    loopTrees, 
                    child);
            curriter.insert(
                    curriter.end(), 
                    loops.begin(), 
                    loops.end());
            continue;
        
        /* 
         * case 2: if we are not in the current loop, 
         * we should return what we have so far
         */
        } else if(currloop->nodes.find(bbname) == currloop->nodes.end()) {
            eptr = make_shared<element>(
                    entryindex, 
                    funcname, 
                    iterationCnt++); 
            eptr->funcs.push_back(curriter);
            curriter.clear();
            fixIterations(iterations, eptr);
            iterations.push_back(eptr);
            return iterations;

        /*
         * case 3: we have gone through the back edge
         */
        } else if(curriter.size() > 0 
                && bbname == currloop->entry) {
            eptr = make_shared<element>(
                    entryindex, 
                    funcname, 
                    iterationCnt++);
            eptr->funcs.push_back(curriter);
            curriter.clear();
            fixIterations(iterations, eptr);
            iterations.push_back(eptr);
        }
        /* 
         * else
         */
        if(traces[index].size() == 4) {
            eptr = make_shared<element>(
                    isEntry, 
                    isExit, 
                    stoi(traces[index][2]), 
                    traces[index][0], 
                    stoul(traces[index][3]));
        } else {
            eptr = make_shared<element>(
                    isEntry, 
                    isExit, 
                    stoi(traces[index][2]), 
                    traces[index][0]);
        }

        fixIterations(curriter, eptr);
        curriter.push_back(eptr);
        index++;
        if(index >= traces.size()) {
            break;
        }

        while(!isExit 
                && index < traces.size() 
                && traces[index][1] == "entry") {
            eptr->funcs.push_back(
                    makeHierarchyLoop(
                        traces, 
                        index, 
                        loopTrees)); 
        }
    } 
    MPI_ASSERT(!isExit);

    // we could be here because we have reached the end of the trace
    eptr = make_shared<element>(
            entryindex, 
            funcname, 
            iterationCnt++);
    /* if(eptr->bb() == "neighbor_communication:neither:3:loop") { */
    /*     int rank; */ 
    /*     MPI_Comm_rank(MPI_COMM_WORLD, &rank); */
    /*     if(rank == 4) { */
    /*         cerr << "found!!\n" << eptr->bb() << ":" << iterationCnt - 1 << endl; */
    /*     } */
    /* } */
    eptr->funcs.push_back(curriter);
    fixIterations(iterations, eptr);
    iterations.push_back(eptr);
    
    return iterations; 
}

/*
 * this function signature takes loops into account
 */
vector<shared_ptr<element>> makeHierarchyMain(
        vector<vector<string>>& traces, 
        unsigned long &index, 
        unordered_map<string, loopNode *>& loopTrees) {
    vector<shared_ptr<element>> functionalTraces;
    bool isEntry = false, 
         isExit = false;
    string funcname = "main";
    MPI_ASSERT(loopTrees.find("main") != loopTrees.end());
    loopNode *loopTree = loopTrees[funcname], 
             *child = nullptr;
    string bbname;
    /*
     * in case there are some nodes that is before main, 
     * just ignore
     */
    while(index < traces.size() 
            && funcname != traces[index][0]) {
        index++;
    }
    while(!isExit 
            && index < traces.size()) {
        MPI_EQUAL(traces[index][0], funcname);
        MPI_ASSERT(traces[index].size() == 3 
                || traces[index].size() == 4);
        bbname = traces[index][0] + ":" 
            + traces[index][1] + ":" 
            + traces[index][2];

        /*
         * this returns the ptr of a child that has bbname in it
         * if none, then it returns nullptr
         */
        if((child = isNewLoop(bbname, loopTree)) != nullptr) {
            vector<shared_ptr<element>> loops = __makeHierarchyLoop(
                    traces, 
                    index, 
                    loopTrees, 
                    child);
            functionalTraces.insert(
                    functionalTraces.end(), 
                    loops.begin(), 
                    loops.end());
            continue;
        }
        isEntry = (traces[index][1] == "entry");
        isExit = (traces[index][1] == "exit");
        shared_ptr<element> eptr;
        if(traces[index].size() == 4) {
            eptr = make_shared<element>(
                    isEntry, 
                    isExit, 
                    stoi(traces[index][2]), 
                    traces[index][0], 
                    stoul(traces[index][3]));
        } else {
            eptr = make_shared<element>(
                    isEntry, 
                    isExit, 
                    stoi(traces[index][2]), 
                    traces[index][0]);
        }
        index++;

        while(!isExit 
                && index < traces.size() 
                && traces[index][1] == "entry") {
            eptr->funcs.push_back(
                    makeHierarchyLoop(
                        traces, 
                        index, 
                        loopTrees));
        }
        fixIterations(functionalTraces, eptr);
        functionalTraces.push_back(eptr);
    }
    return functionalTraces;
    
}

/*  
 *  1. Every function starts from entry except for main 
 *  2. Let's not take anything in unless the main function has started
 *  3. Let's not take anything in after the main function has ended
 *  4. Even if the function name is the same, 
 *      if we reach the new entry node, we recursively call the function
 */
vector<shared_ptr<element>> makeHierarchy(
        vector<vector<string>>& traces, unsigned long& index) {
    vector<shared_ptr<element>> functionalTraces;
    bool isEntry, isExit;
    string bbname, funcname;
    funcname = traces[index][0];
    do {
        MPI_ASSERT(index < traces.size());
        bbname = traces[index][0] + ":" + traces[index][1] + ":" + traces[index][2];
        MPI_ASSERT(traces[index][0] == funcname);
        MPI_ASSERT(traces[index].size() == 3 
                || traces[index].size() == 4);
        isEntry = (traces[index][1] == "entry");
        isExit = (traces[index][1] == "exit");
        shared_ptr<element> eptr;
        if(traces[index].size() == 4) {
            eptr = make_shared<element>(
                    isEntry, 
                    isExit, 
                    stoi(traces[index][2]), 
                    traces[index][0], 
                    stoul(traces[index][3]));
        } else {
            eptr = make_shared<element>(
                    isEntry, 
                    isExit, 
                    stoi(traces[index][2]), 
                    traces[index][0]);
        }
        index++;
        while(!isExit 
                && index < traces.size() 
                && traces[index][1] == "entry") {
            eptr->funcs.push_back(
                    makeHierarchy(traces, index)); 
        }
        fixIterations(functionalTraces, eptr);
        functionalTraces.push_back(eptr);
    } while(!isExit 
            && index < traces.size());

    return functionalTraces; 
}

/*
 * checks if the bbname is the entry of the loop with parent's node
 */
static inline bool isLoopEntry(
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

void addHierarchy(
        vector<shared_ptr<element>>& functionalTraces, 
        vector<vector<string>>& traces, 
        unsigned long& index, 
        unordered_map<string, loopNode *>& loopTrees) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    stack<shared_ptr<element>> __stack;
    MPI_ASSERT(!functionalTraces.empty());
    MPI_EQUAL(functionalTraces[0]->funcname, "main");
    MPI_EQUAL(functionalTraces.back()->funcname, "main");
    shared_ptr<element> curr = functionalTraces.back();
    loopNode *currloop = nullptr, 
             *tmploop = nullptr;

    /* 
     * we are first setting the cursor to the end of the functionalTraces here
     * we are pushing to a stack of the elements in the back too
     */
    while(!(curr->isExit)) {
        __stack.push(curr);
        //DEBUG0("curr->bb(): %s in the stack\n", curr->bb().c_str());
        if(curr->funcs.size() == 0) {
            break;
        } else {
            curr = curr->funcs.back().back();
        }
    }

    /*
     * we are setting the parent to the right value
     * we take out the parent values from the stack too
     */
    shared_ptr<element> parent, newchild;
    MPI_ASSERT(!__stack.empty());
    curr = __stack.top();
    __stack.pop();
    if(__stack.empty()) {
        parent = nullptr;
    } else {
        parent = __stack.top();
        __stack.pop();
    }

    /* 
     * initializing to the correct currloop
     */
    currloop = loopTrees[curr->funcname];
    while((tmploop = isNewLoop(curr->bb(), currloop)) 
            != nullptr) {
        currloop = tmploop;
    }

    while(index < traces.size()) {
        /* 
         * if we are at the entry of a new function 
         * (we assume that there's no loop to the entry here),
         * we need to make a hierarchy of the new function
         */
        while(index < traces.size() 
                && traces[index][1] == "entry") {
            curr->funcs.push_back(
                    makeHierarchyLoop(
                        traces, 
                        index, 
                        loopTrees));
        }
        if(index >= traces.size()) break;
        MPI_ASSERT(traces[index].size() > 0);
        string bb = traces[index][0] + ":" + traces[index][1] + ":" + traces[index][2]; 
        
        /* 
         * we need to think of two cases in case we are in the entry node of a current loop 
         * 1. we are already in the loop
         * 2. we are newly in the loop
         * in either case, it is taken care of by __makeHierarchyLoop
         */
        if(isLoopEntry(bb, parent, currloop)) {
            /* 
             * implement the case where we have to 
             * go through the current loop
             */
            /* if(parent->funcname != curr->funcname) { */
                /* cerr << parent->bb() << " " << curr->bb() << endl; */
                /* print(replayTraces, 0); */
            /* } */
            MPI_EQUAL(parent->funcname, curr->funcname);
            MPI_ASSERT(parent->isLoop == true);
            /*
             * CAUTION: this will return the iterations of the loop, and they are at
             * the same level as the parent, not the children
             */
            int iterationCnt = parent->index + 1;
            vector<shared_ptr<element>> iterations = 
                __makeHierarchyLoop(
                        traces, 
                        index, 
                        loopTrees, 
                        currloop,
                        iterationCnt);
            if(!__stack.empty()) {
                /*
                 * inserting the loop iteration as a children of 
                 * the top of the stack
                 * notice, they are at the level of the children, 
                 * and not the parents
                 */
                __stack.top()->funcs.back().insert(
                        __stack.top()->funcs.back().end(), 
                        iterations.begin(), 
                        iterations.end());
                curr = parent;
                parent = __stack.top();
                __stack.pop();
            } else {
                /*
                 * if there are no parents anymore, 
                 * since they are also at the same level
                 * the parent, we need to directly insert 
                 * to functionalTraces here
                 */
                functionalTraces.insert(
                        functionalTraces.end(), 
                        iterations.begin(), 
                        iterations.end());
                curr = parent;
                parent = nullptr;
            }

            MPI_ASSERT(currloop->nodes.size() > 0 
                    && currloop->parent);
            currloop = currloop->parent;
        }

        if(index >= traces.size()) break;

        bb = traces[index][0] 
            + ":" + traces[index][1] 
            + ":" + traces[index][2];
        /*
         * we assert that current loop node has some nodes in it, or a dummy loop node
         */
        MPI_ASSERT(currloop->nodes.size() > 0 
                || loopTrees[curr->funcname] == currloop);
        /*
         * if we are going into a new loop here
         */
        while((tmploop = isNewLoop(bb, currloop)) 
                != nullptr) {
            /*
             * here you will create a loop iterations through 
             * __makeHierarchyLoop
             * but this iterations are at the same level 
             * as the children of the parent
             */
            vector<shared_ptr<element>> loops 
                = __makeHierarchyLoop(
                        traces, 
                        index, 
                        loopTrees, 
                        tmploop);
            /*
             * since these iterations are the same level 
             * as children of the parent,
             * we push it as the children of the same parent here
             */
            if(parent != nullptr) {
                parent->funcs.back().insert(
                        parent->funcs.back().end(), 
                        loops.begin(), 
                        loops.end());
            } else {
                /* 
                 * if there are no parents, 
                 * we directly insert to functionalTraces
                 */
                functionalTraces.insert(
                        functionalTraces.end(), 
                        loops.begin(), 
                        loops.end());
            }
            if(index >= traces.size()) break;
            bb = traces[index][0] + ":" + traces[index][1] + ":" + traces[index][2];
        }

        if(index >= traces.size()) break;
        /*
         * the node cannot be an entry, 
         * because if it is, we should be at the beginning of 
         * this function
         */
        MPI_ASSERT(traces[index][1] != "entry");
        /* 
         * here we check if we are getting out of this loop
         * while we are, we need to update the currloop 
         * as well as curr, parent, and stack
         */
        while(currloop->nodes.size() > 0 
                && currloop->nodes.find(bb) == currloop->nodes.end()) {
            /*
             * because of the condition above, 
             * we made sure that we are in the loop in curr
             * hence parent has to be a loop node
             */
            MPI_ASSERT(parent != nullptr);
            MPI_ASSERT(parent->isLoop);
            currloop = currloop->parent;
            curr = parent;
            if(!__stack.empty()) {
                parent = __stack.top();
                __stack.pop();
            } else {
                parent = nullptr;
            }
            MPI_ASSERT(curr->isLoop);
        }

        /*
         * we create a new element here
         */
        if(traces[index].size() == 3) {
            newchild = make_shared<element>(
                    traces[index][1] == "entry", 
                    traces[index][1] == "exit", 
                    stoi(traces[index][2]), 
                    traces[index][0]);
        } else {
            MPI_ASSERT(traces[index].size() == 4);
            newchild = make_shared<element>(
                    traces[index][1] == "entry", 
                    traces[index][1] == "exit", 
                    stoi(traces[index][2]), 
                    traces[index][0], 
                    stoul(traces[index][3]));
        }
        MPI_ASSERT(traces[index][0] == curr->funcname);
        index++;
    
        if(parent != nullptr) {
            /*
             * we initialize the vector, in case we don't have any value in them
             * then we push back the new child we just created
             */
            if(parent->funcs.size() == 0) {
                parent->funcs.push_back(
                        vector<shared_ptr<element>>());
            }
            /*
             * making sure that the loop index is correct
             */
            fixIterations(parent->funcs.back(), newchild);
            parent->funcs.back().push_back(
                    newchild);
        } else {
            /*
             * in case there's no parents, 
             * you push back directly to functionalTraces
             */
            fixIterations(functionalTraces, newchild);
            functionalTraces.push_back(newchild);
        }
        
        /*
         * if we are ending this batch, 
         * it should be either a loop iteration, or a function
         * just add a condition || and add the condition 
         * of the end of the loop
         */
        if(index >= traces.size()) {
            break;
        }
     
        /*
         * this is the next node to be taken care of
         * we try to break with a code block before 
         * in case we don't have one anymore
         */
        bb = traces[index][0] + ":" + traces[index][1] + ":" + traces[index][2];

        /*
         * if the last node was an exit node, 
         * then we need to update the curr, parent, and stack
         */
        if(traces[index - 1][1] == "exit") {
            if(parent == nullptr) {
                /* 
                 * if we don't have a parent anymore, 
                 * and the last node was an exit node
                 * then this node should be the main 
                 * and we should not have anymore nodes 
                 * to be recorded
                 */
                MPI_ASSERT(traces[index - 1][0] == "main");
                break;
            } else {
                curr = parent;
                if(!__stack.empty()) {
                    parent = __stack.top();
                    __stack.pop();
                } else {
                    parent = nullptr;
                }
            }
            /*
             * need to update the looptrees as well 
             */
            currloop = loopTrees[curr->funcname];
            while((tmploop = isNewLoop(curr->content(), currloop)) 
                    != nullptr) {
                currloop = tmploop;
            }
            if(currloop->entry == bb) {
                /* 
                 * the situation is... 
                 * 1. the last node was an exit bb of the function
                 * 2. and now the current trace is an entry 
                 * of the loop
                 * if the parent is the virtual node 
                 * of this loop, you need to make another virtual
                 * node for this iterations
                 * if not, you stil need to make another virtual 
                 * node
                 */
                if(parent->isLoop == true
                        && parent->bb() == currloop->entry + ":loop") {
                    auto loopIterations = __makeHierarchyLoop(
                            traces, 
                            index, 
                            loopTrees, 
                            currloop,
                            parent->index + 1);     
                    curr = parent;
                    if(!__stack.empty()) {
                        parent = __stack.top();
                        __stack.pop();
                        parent->funcs.back().insert(
                                parent->funcs.back().end(), 
                                loopIterations.begin(), 
                                loopIterations.end());
                    } else {
                        parent = nullptr;
                        functionalTraces.insert(
                                functionalTraces.end(), 
                                loopIterations.begin(), 
                                loopIterations.end());
                    }
                } else {
                    cerr << "we are not dealing with this case yet\n";
                    MPI_ASSERT(false);
                }
            }
        } else {
            /*
             * otherwise, simply update the curr to the new child
             */
            curr = newchild;
        }
    }
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

// this sets the pointer to the beginning of the file
static size_t findSize(FILE *fp) {
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    return size;
}

deque<shared_ptr<lastaligned>> onlineAlignment(
        deque<shared_ptr<lastaligned>>& q, 
        bool& isAligned, 
        size_t &lastInd, 
        unordered_map<string, loopNode *>& loopTrees) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    appendTraces(
            loopTrees, 
            replayTracesRaw, 
            replayTraces);
    
    size_t i = 0, j = 0; 
    size_t funcId = 0; 
    if(!q.empty()) { 
         i = q.back()->origIndex; 
         j = q.back()->repIndex; 
         funcId = q.back()->funcId; 
         q.pop_back(); 
    } 
    deque<shared_ptr<lastaligned>> rq;
    isAligned = true; 
    
    // there's no parent for original nor the reproduced, so kept empty
    rq = greedyalignmentOnline(
            recordTraces, 
            replayTraces, 
            q, 
            i, 
            j, 
            funcId, 
            rank, 
            isAligned, 
            lastInd); 
    MPI_ASSERT(lastInd != numeric_limits<size_t>::max()); 
    /* MPI_ASSERT(lastInd > 0); */
    return rq;
}

void appendReplayTrace(
        unordered_map<string, loopNode *>& loopTrees) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    static unsigned long index = 0;
    if(replayTraces.size() == 0) {
        MPI_ASSERT(index == 0);
        replayTraces = makeHierarchyMain(
                replayTracesRaw, 
                index, 
                loopTrees);
    } else {
        addHierarchy(
                replayTraces, 
                replayTracesRaw, 
                index, 
                loopTrees);
    }

    return;
}

void appendTraces(
        unordered_map<string, loopNode *>& loopTrees,
        vector<vector<string>>& rawTraces,
        vector<shared_ptr<element>>& traces) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    static unsigned long index = 0;
    if(traces.size() == 0) {
        MPI_ASSERT(index == 0);
        traces = makeHierarchyMain(
                rawTraces, 
                index, 
                loopTrees);
    } else {
        addHierarchy(
                traces, 
                rawTraces, 
                index, 
                loopTrees);
    }
    return;
}

void appendTraces(
        unordered_map<string, loopNode *>& loopTrees,
        TraceType traceType) {
    if(traceType == TraceType::RECORD) {
        appendTraces(
                loopTrees, 
                recordTracesRaw, 
                recordTraces);
    } else if (traceType == TraceType::REPLAY) {
        appendTraces(
                loopTrees, 
                replayTracesRaw, 
                replayTraces);
    }
}

static string getLastNodes(vector<shared_ptr<element>>& traces) {
    MPI_ASSERT(traces.size() > 0);
    stringstream ss;
    shared_ptr<element> curr = traces.back();
    int funcId = 0;
    do {
        ss << '/' << funcId << ':' << curr->bb(); 
        if(curr->loopIndex != -1) {
            ss << ':' << curr->loopIndex; 
        }
        if(curr->isLoop) {
            ss << ':' << curr->index; 
            MPI_ASSERT(curr->loopIndex == -1);
        }
        if(curr->funcs.size() == 0) {
            MPI_ASSERT(!curr->isLoop);
            break;
        } else {
            funcId = curr->funcs.size() - 1;
            curr = curr->funcs.back().back();
            MPI_ASSERT(funcId >= 0);
        }
    } while (true);
    return ss.str();
}

string getLastNodes(TraceType traceType) {
    if(traceType == TraceType::RECORD) {
        return getLastNodes(recordTraces);
    } else if (traceType == TraceType::REPLAY) {
        return getLastNodes(replayTraces);
    }
    return "";
}

string getVeryLastNode(TraceType traceType) {
    vector<string> vec;
    if(traceType == TraceType::RECORD) {
        vec = recordTracesRaw.back();
    } else if (traceType == TraceType::REPLAY) {
        vec = replayTracesRaw.back();
    }
    return vec[0] + ":" + vec[1] + ":" + vec[2];
}

string updateAndGetLastNodes(
        unordered_map<string, loopNode *>& loopTrees, 
        TraceType traceType) {
    appendTraces(loopTrees, traceType);
    return getLastNodes(traceType);
}

static inline void create_map(
        unordered_map<string, vector<size_t>>& bbMap, 
        vector<shared_ptr<element>>& reproduced, 
        size_t& j) {
    for(unsigned k = j; k < reproduced.size(); k++) {
        if(bbMap.find(reproduced[k]->bb()) == bbMap.end()) {
            bbMap[reproduced[k]->bb()] = vector<size_t>();
        }
        bbMap[reproduced[k]->bb()].push_back(k);
    }
    return;
}

/*
 * this function just checks that 
 * there are some basic blocks that aligns till the end
 * it does NOT verify that it might have the divergence afterwards
 */
static inline bool matchbbmap(
        unordered_map<string, vector<size_t>>& bbMap, 
        vector<shared_ptr<element>>& original, 
        size_t& i, 
        size_t& j) {
    for(unsigned k = i; k < original.size(); k++) {
        if(bbMap.find(original[k]->bb()) != bbMap.end()) {
            for(auto l: bbMap[original[k]->bb()]) {
                if(j <= l) {
                    i = k;
                    j = l;
                    return true;
                }
            }
        } 
    }
    return false;
}

void getProperLastInd(size_t& lastInd, shared_ptr<element> curr) {
    while(curr->isLoop == true) {
        MPI_ASSERT(curr->funcs.size() == 1);
        curr = curr->funcs.back().front();
    }
    lastInd = max(lastInd, curr->index);
    MPI_ASSERT(lastInd != numeric_limits<size_t>::max());
}
/*
 * This function is the core of the alignment when they are in the same function
 * or in the same loop
 * when the misalignment is found, we first fill up the map of the traces of the
 * reproduced execution, and then we try to match the traces in the original 
 * exection. 
 * CAUTION: we do not have to think about the loop iterations here, because that's
 * already taken care of
 */
static pair<size_t, size_t> __greedyalignmentOnline(
        vector<shared_ptr<element>>& original, 
        vector<shared_ptr<element>>& reproduced, 
        vector<pair<shared_ptr<element>, shared_ptr<element>>>& aligned, 
        size_t& initI, 
        size_t& initJ, 
        const int& rank, 
        bool& isAligned, 
        size_t& lastInd,
        bool isLoop = false) {
    /*
     * we set the i and j to be at the initial values so that we don't have to find 
     * the same points of divergence redundantly.
     */
    size_t i = initI, j = initJ;
    /*
     * we initialize bbMap here in case the misalignment happens
     */
    unordered_map<string, vector<size_t>> bbMap;
    /* if(original[0]->funcname == "hypre_FinalizeCommunication" && rank == 3) { */
        /* cerr << "original\n"; */
        /* printsurface(original); */
        /* cerr << "reproduced\n"; */
        /* printsurface(reproduced); */
    /* } */
    while(i < original.size() 
            && j < reproduced.size()) {
        if(original[i]->bb() == reproduced[j]->bb()) {
            /*
             * in case the nodes are the same, we push them to the aligned vector
             * and increment the i and j to move onto the next nodes for checking 
             * alignments
             */
            aligned.push_back(
                    make_pair(original[i], reproduced[j]));
            i++; j++;
        } else {
            /*
             * in case the nodes are not the same, we first fill up the map of the 
             * reproduced traces (in case they are not filled up already), 
             * and then we try to match the traces in the original execution
             */
            string pod;
            if(original[i - 1]->isLoop == true) {
                pod = original[i - 1]->bb() 
                    + ':' 
                    + to_string(original[i - 1]->index);
                /*
                if(pods.find(pod) == pods.end()) {
                    printf("pods: %s, rank: %d\n", pod.c_str(), rank);
                }
                */
                pods.insert(pod);
            } else {
                pod = original[i - 1]->bb();
                if(pods.find(pod) == pods.end()) {
                    printf("pods: %s, rank: %d\n", pod.c_str(), rank);
                }
                pods.insert(pod);
            }
            
            size_t oldj = j, oldi = i;

            /*
             * filling up the bbMap for the reproduced execution here
             */
            if(bbMap.size() == 0) {
                create_map(bbMap, reproduced, j);
            }
            if(!matchbbmap(bbMap, original, i, j)) {
                /*
                 * here we never aligned until the end
                 * so we set the lastInd to the last index of the original execution 
                 */
                getProperLastInd(lastInd, original[oldi]);
            
                /* if(rank == 3) { */
                    /* cerr << "returning lastInd: " << lastInd << endl; */
                /* } */
                
                MPI_ASSERT(lastInd != numeric_limits<size_t>::max());
                isAligned = false;
                return make_pair(oldi, oldj);
            } else {
                /*
                 * if we find the same basic block here
                 * we have already set the i and j to the point of convergence,
                 * so move on to the next nodes by incrementing i and j
                 */
                /* printf("poc: %s, rank: %d\n", original[i]->bb().c_str(), rank); */
                i++; j++;
            }
        }
    }

    isAligned = true;

    /*
     * if we have some nodes in original, either because reproduced nodes have not finished
     * or because we are in the loop, and original simply have more amount of nodes in the
     * iteration, we need to return the last aligned point
     */
    if(i < original.size()) {
        getProperLastInd(lastInd, original[i - 1]);
        MPI_ASSERT(lastInd != numeric_limits<size_t>::max());
        /* if(rank == 3) { */
            /* cerr << "i < original.size() returning lastInd: " << lastInd << endl; */
        /* } */
        return make_pair(i - 1, j - 1);

    /*
     * Or, if this alignment is done on loop iteration, we could have more reproduced nodes,
     * in this case, we assure that this is aligned on a loop iteration 
     * since this is the end of the alignment, we should make the return value to be the
     * max
     */
    } else if (j < reproduced.size()) {
        MPI_ASSERT(isLoop == true);
        getProperLastInd(lastInd, original[i - 1]);
        // do we really need this?
        MPI_ASSERT(lastInd != numeric_limits<size_t>::max());
        /* if(rank == 3) { */
            /* cerr << "j < reproduced.size() returning lastInd: " << lastInd << endl; */
        /* } */
        return make_pair(
                numeric_limits<size_t>::max(), numeric_limits<size_t>::max());

    /* 
     * when both original and reproduced vectors are aligned at the end
     * we return the max value for both i and j
     */
    } else {
        MPI_ASSERT(i == original.size()
                && j == reproduced.size());
        getProperLastInd(lastInd, original[i - 1]);
        // do we really need this?
        MPI_ASSERT(lastInd != numeric_limits<size_t>::max());
        /* if(rank == 3) { */
            /* cerr << "i == original.size() && j == reproduced.size() returning lastInd: " << lastInd << endl; */
        /* } */
        return make_pair(
                numeric_limits<size_t>::max(), numeric_limits<size_t>::max());
    }
}

static bool issuccessful(pair<size_t, size_t> p) {
    return p.first == numeric_limits<size_t>::max() 
        && p.second == numeric_limits<size_t>::max();
}

// this function is just to make sure
static bool isLastAligned(
        vector<shared_ptr<element>>& original, 
        vector<shared_ptr<element>>& reproduced, 
        size_t i, 
        size_t j) {
    unordered_map<string, vector<size_t>> bbMap;
    create_map(bbMap, reproduced, j);
    return matchbbmap(
            bbMap, 
            original, 
            i, 
            j);
}

/*
 * this function is the outside of the greedy alignment, while considering the loop iterations
 * it compares the two hierarchical traces: original and reproduced
 * it returns the last aligned point of both original and reproduced by dequeue, so that one can exactly know where it is
 * this function is also recursive, in case it needs to delve into the functions inside or a loop inside
 * below is the high level steps, it take
 * 1. it first checks for the queues passed down as an argument, and see if there's any misalignment there 
 * 2. CASE 1: if there is one, we return the queue partially as it was passed,
 *      partially where it left off
 * 3. CASE 2: if there is no misalignment, we check for the last misalignment that happened 
 *      at the same basic block, but at a different function call
 * 4. CASE 3: if there's no misalignment in CASE 2, we check for the last misalignment 
 *      at this function.
 * 5. after the last point of alignment is found, we return the dequeue as a stack of 
 *      where we left off (for ALL cases)
 */
deque<shared_ptr<lastaligned>> greedyalignmentOnline(
        vector<shared_ptr<element>>& original, 
        vector<shared_ptr<element>>& reproduced, 
        deque<shared_ptr<lastaligned>>& q, 
        size_t &i, 
        size_t &j, 
        const size_t& funcId, 
        const int &rank, 
        bool& isAligned, 
        size_t& lastInd,
        shared_ptr<element> originalParent,
        shared_ptr<element> reproducedParent) {
    MPI_EQUAL(original[i]->funcname, reproduced[j]->funcname);
    // if the queue is not empty, let's do the alignment level below first
    deque<shared_ptr<lastaligned>> rq;
    /* DEBUG0("greedyalignmentOnline at %s with i: %lu, j: %lu, funcId:%lu\n",\ */
            /* original[i]->bb().c_str(), i, j, funcId); */

    // in case the q is not empty, we need to do the alignment level below first
    bool firstTime = false;
    /*
     * Here, we are checking if the queue is empty, if not, go into the queue and find the last
     * aligned point, and then do the alignment levels below with recursion
     */
    if(!q.empty()) {
        shared_ptr<lastaligned> la = q.back();
        if(la->origIndex == numeric_limits<size_t>::max() 
                && la->repIndex == numeric_limits<size_t>::max()) {
            // so we check if the next one has been completed
            q.clear();
        } else {
            q.pop_back();
            size_t tmpfuncId = la->funcId;
            if(tmpfuncId == numeric_limits<size_t>::max()) {
                tmpfuncId = 0;
            }
            size_t tmpLastInd = 0;
            MPI_EQUAL(
                    original[i]->funcs[funcId][0]->funcname, 
                    reproduced[j]->funcs[funcId][0]->funcname);
            /*
             * We are calling greedyalignment online here recursively
             * where one level of stack is popped and used as arguments
             * this is CASE 1
             */
            rq = greedyalignmentOnline(
                    original[i]->funcs[funcId], 
                    reproduced[j]->funcs[funcId], 
                    q, 
                    la->origIndex, 
                    la->repIndex, 
                    tmpfuncId, 
                    rank, 
                    isAligned, 
                    tmpLastInd,
                    original[i],
                    reproduced[j]);
            lastInd = max(lastInd, tmpLastInd);
        }
    } else {
        firstTime = true;
    }

    /* 
     * If the recursive call of greedyalignmentOnline returns with rq that is not empty
     * that means there was an unresolved misalignment, since we only have one return call
     * for every control flow graphs, these conditions suffice as not going further down 
     * in this function
     */
    if(!rq.empty() 
            && !(rq.back()->isSuccess())) {
        /* DEBUG0("at %s, we did not proceed this time 1, \ */
                /* funcId: %lu, i: %lu, j: %lu\n", \ */
                /* original[i]->bb().c_str(), funcId, i, j); */
        MPI_ASSERT(
                isLastAligned(
                    original, 
                    reproduced, 
                    i, 
                    j));
        /* getProperLastInd(lastInd, original[i]); */
        rq.push_back(
                make_shared<lastaligned>(
                    funcId, 
                    i, 
                    j));
        return rq;
    }

    // we expect the q to be empty through recursion
    MPI_ASSERT(q.empty());

    // in case functions were at the same element, we do the alignment the level below too
    
    size_t k = -1;
    /*
     * this is CASE 2, where we need to look into the same basic blocks, but in the following
     * function calls, we basically do the same as the code block above,
     * except in this case, we no longer have to worry about the stack that's passed down from 
     * parent functions.
     */
    for(k = funcId + 1; k < reproduced[j]->funcs.size(); k++) {
        /*
         * popping unnecessary data from rq, as we have gotten through the last call on
         * greedyalignmentOnline
         * TODO: this can be written more clearly, 
         * like why are you filling in the garbage in the first place?
         */
        while(!rq.empty()) {
            MPI_ASSERT(rq.back()->isSuccess());
            rq.pop_back();
        }
        size_t tmpI = 0, tmpJ = 0, tmpLastInd = 0;
        /*
         * below incorporates the case different functions being called through function pointers
         */
        if(original[i]->funcs[k][0]->funcname != reproduced[j]->funcs[k][0]->funcname) {
            MPI_ASSERT(original[i]->isLoop == false);
            pods.insert(original[i]->bb());
            /*
            printf("different function called from %s: %s vs. %s\n", 
                    original[i]->bb().c_str(), 
                    original[i]->funcs[k][0]->funcname.c_str(), 
                    reproduced[j]->funcs[k][0]->funcname.c_str());
            */
        } else {
            rq = greedyalignmentOnline(
                    original[i]->funcs[k], 
                    reproduced[j]->funcs[k], 
                    q, 
                    tmpI, 
                    tmpJ, 
                    k, 
                    rank, 
                    isAligned, 
                    tmpLastInd,
                    original[i],
                    reproduced[j]);
        }
        lastInd = max(lastInd, tmpLastInd);
        MPI_ASSERT(lastInd > 0);
        MPI_ASSERT(((rq.empty() || rq.back()->isSuccess()) && isAligned) || \
                (k == reproduced[j]->funcs.size() - 1));
    }

    /*
     * Here, it's the same logic as above: if there are some misalignment ended as misalignment,
     * one has to return back because the last point of misalignment is already found
     */
    if(!rq.empty() && !(rq.back()->isSuccess())) {
        MPI_ASSERT(isLastAligned(original, reproduced, i, j));
        // this logic is not correct, like funcs.size() would overflow
        rq.push_back(make_shared<lastaligned>(reproduced[j]->funcs.size() - 1, i, j));
        MPI_ASSERT(lastInd > 0);
        return rq;
    }

    /*
     * let's do the greedy alignment from where we left off in this function, below is CASE 3.
     */
    vector<pair<shared_ptr<element>, shared_ptr<element>>> aligned = 
        vector<pair<shared_ptr<element>, shared_ptr<element>>>();

    /* 
     * in case we are doing this for the first time, we should start at i and j 
     * however, if not, then you should start at the i + 1 and j + 1
     */
    pair<size_t, size_t> p;
    size_t tmpLastInd = 0;
    bool isLoop = (originalParent != nullptr
            && reproducedParent != nullptr
            && originalParent->isLoop
            && reproducedParent->isLoop);
    if(firstTime == true) {
        MPI_ASSERT(original[i]->funcname == reproduced[j]->funcname);
        MPI_ASSERT((originalParent == nullptr 
                    && reproducedParent == nullptr)
                || (originalParent->isLoop == reproducedParent->isLoop));
        p = __greedyalignmentOnline(
                original, 
                reproduced, 
                aligned, 
                i, 
                j, 
                rank, 
                isAligned, 
                tmpLastInd,
                isLoop);
        /* MPI_ASSERT(tmpLastInd > 0); */
    } else {
        size_t tmpI = i + 1, tmpJ = j + 1;
        /* DEBUG0("tmpi: %lu, tmpj: %lu, original.size(): %lu, reproduced.size(): %lu\n", tmpi, tmpj, original.size(), reproduced.size()); */
        /*
         * tmpi could be equal to original.size() if the parent is a loop node
         * tmpj could be equal to reproduced.size() if the parent is a loop node or at the end 
         * of the trace
         */
        if(tmpJ == reproduced.size()) {
            p = make_pair(i, j);
            /*
             * below might be wrong
             */
            /* getProperLastInd(lastInd, original[i]); */
        } else {
            if(tmpI == original.size()) {
                // this happens when the parents are loop node
                MPI_ASSERT(originalParent != nullptr
                        && reproducedParent != nullptr
                        && originalParent->isLoop 
                        && reproducedParent->isLoop);
                // we keep the point of last alignment as is
                p = make_pair(i, j);
                /* getProperLastInd(lastInd, original[i]); */
            } else if (tmpJ == reproduced.size()) {
                // we should check if we have the same parent loop node
                // below is a temporary solution, as we need to use the last node
                MPI_ASSERT((originalParent != nullptr
                        && reproducedParent != nullptr
                        && originalParent->isLoop
                        && reproducedParent->isLoop)
                        || (true));
                // we keep the point of last alignment as is
                p = make_pair(i, j);
                /* getProperLastInd(lastInd, original[i]); */
            } else {
                MPI_ASSERT(original[tmpI]->funcname == reproduced[tmpJ]->funcname);
                MPI_ASSERT((originalParent == nullptr 
                            && reproducedParent == nullptr)
                        || originalParent->isLoop == reproducedParent->isLoop);
                p = __greedyalignmentOnline(
                        original, 
                        reproduced, 
                        aligned, 
                        tmpI, 
                        tmpJ, 
                        rank, 
                        isAligned, 
                        tmpLastInd,
                        isLoop);
            }
        }
    }
    lastInd = max(lastInd, tmpLastInd);

    deque<shared_ptr<lastaligned>> qs;
    // if we found some points that are aligned, let's do the alignment level below
    bool tmpIsAligned = true;
    // for each aligned pair (the 1st for loop), and for each function inside, we need to do the alignment
    size_t newfuncId;
    for(unsigned ind0 = 0; ind0 < aligned.size(); ind0++) {
        MPI_ASSERT(aligned[ind0].first->bb() == aligned[ind0].second->bb());
        MPI_ASSERT(aligned[ind0].first->funcs.size() 
                == aligned[ind0].second->funcs.size() 
                    || ind0 == aligned.size() - 1);
        MPI_ASSERT(aligned[ind0].first->funcs.size() 
                >= aligned[ind0].second->funcs.size());
        newfuncId = aligned[ind0].second->funcs.size() - 1;
        tmpLastInd = 0;
        for(unsigned ind1 = 0; ind1 < aligned[ind0].second->funcs.size(); ind1++) {
            size_t tmpi = 0, 
                   tmpj = 0, 
                   tmpfuncId = 0;
            if(aligned[ind0].first->funcs[ind1][0]->funcname 
                    != aligned[ind0].second->funcs[ind1][0]->funcname) {
                /* printf("different function called from %s: %s vs. %s\n", \ */
                        /* aligned[ind0].first->bb().c_str(), \ */
                        /* aligned[ind0].first->funcs[ind1][0]->funcname.c_str(), \ */
                        /* aligned[ind0].second->funcs[ind1][0]->funcname.c_str()); */
            } else {
                /* MPI_ASSERT(aligned[ind0].first->funcs[ind1][0]->funcname == aligned[ind0].second->funcs[ind1][0]->funcname); */
                qs = greedyalignmentOnline(
                        aligned[ind0].first->funcs[ind1], 
                        aligned[ind0].second->funcs[ind1], 
                        q, 
                        tmpi, 
                        tmpj, 
                        tmpfuncId, 
                        rank, 
                        tmpIsAligned, 
                        tmpLastInd,
                        aligned[ind0].first,
                        aligned[ind0].second);
                // we only expect the last of the last thing to be not successful
                // why are we looking at funcids for isSuccess method?
                MPI_ASSERT((qs.back()->isSuccess() && tmpIsAligned) || \
                        (ind0 == aligned.size() - 1 && \
                        ind1 == aligned[ind0].second->funcs.size() - 1));
                MPI_ASSERT(tmpLastInd > 0);
                if(!tmpIsAligned) {
                    isAligned = false;
                }
                lastInd = max(tmpLastInd, lastInd);
            }
        }
    }
    /*
     * if tmp said the alignment was not done at last, 
     * then we should expect the alignment did not finish at p either
     * if that is the case, push them back to the queue 
     */

    rq = qs;
    // funcId is where you left off
    /* DEBUG0("pushing newfuncId: %lu, p.first:%lu, p.second:%lu\n", */ 
    /*         newfuncId, */ 
    /*         p.first, */ 
    /*         p.second); */
    rq.push_back(
            make_shared<lastaligned>(
                newfuncId, 
                p.first, 
                p.second));
    return rq;
}

/*
 * it returns true iff the traces are aligned at the end
 */
static bool __greedyalignmentOffline(
        vector<shared_ptr<element>>& original, 
        vector<shared_ptr<element>>& reproduced, 
        vector<pair<shared_ptr<element>, shared_ptr<element>>>& aligned, 
        const int& rank) {
    size_t i = 0, j = 0;
    unordered_map<string, vector<size_t>> bbMap;
    /* unsigned div = 0; */
    while(i < original.size() && j < reproduced.size()) {
        if(original[i]->bb() == reproduced[j]->bb()) {
            /* fprintf(stderr, "aligned: %s\n", original[i]->bb.c_str()); */
            aligned.push_back(make_pair(original[i], reproduced[j]));
            i++; j++;
        } else {
            //fprintf(stderr, "option 1, rank:%d, pod:%s\n", rank, original[i - 1]->bb().c_str());
            /* div++; */
            if(bbMap.size() == 0) create_map(bbMap, reproduced, j);

            // matchbbmap will update i and j
            if(!matchbbmap(bbMap, original, i, j)) return false;
            else {
                fprintf(stderr, "rank:%d, poc:%s", rank, original[i]->bb().c_str());
                aligned.push_back(make_pair(original[i], reproduced[j]));
                i++; j++;
            }
        }
    }

    if(i < original.size()) {
        //fprintf(stderr, "option 2, rank:%d, pod:%s\n", rank, original[i - 1]->bb().c_str());
        /* div++; */
        return false;
    } else if(j < reproduced.size()) {
        //fprintf(stderr, "option 3, rank:%d, pod:%s\n", rank, reproduced[j - 1]->bb().c_str());
        /* div++; */
        return false;
    } 

    /* DEBUG0("divergence: %u\n", div); */
    /* fprintf(stderr, "divergence: %u\n", div); */
    return true;
}

bool greedyalignmentWholeOffline(
        vector<shared_ptr<element>>& original, 
        vector<shared_ptr<element>>& reproduced, 
        const int& rank) { 
    vector<pair<shared_ptr<element>, shared_ptr<element>>> aligned;
    /* fprintf(stderr, "greedy alignment on %s at %d original.size: %lu, reproduced.size: %lu\n", \ */
            /* original[0]->bb.c_str(), rank, original.size(), reproduced.size()); */
    bool rv = __greedyalignmentOffline(
            original, 
            reproduced, 
            aligned, 
            rank); 
    for(auto p : aligned) {
        /* cout << p.first->bb << " " << p.second->bb << endl; */
        MPI_ASSERT(p.first->bb() == p.second->bb());
        /* MPI_ASSERT(p.first->funcs.size() == p.second->funcs.size()); */
        unsigned long len = p.first->funcs.size() < p.second->funcs.size() ? p.first->funcs.size() : p.second->funcs.size();
        for(unsigned i = 0; i < len; i++) {
            // update rv at the end
            rv = greedyalignmentWholeOffline(
                    p.first->funcs[i], 
                    p.second->funcs[i], 
                    rank);
        }
        if(p.first->funcs.size() != p.second->funcs.size()) {
            DEBUG("at %s, p.first->funcs.size(): %lu, p.second->funcs.size(): %lu\n", 
                    p.first->bb().c_str(), 
                    p.first->funcs.size(), 
                    p.second->funcs.size());
        }
    }
    return rv;
}

bool greedyalignmentWholeOffline() {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    return greedyalignmentWholeOffline(
            recordTraces, 
            replayTraces, 
            rank);
}

// returns an empty vector in case of error
vector<string> getMsgs(
        vector<string> &orders, 
        const size_t lastInd, 
        unsigned& orderIndex,
        string* recSendNodes) {
    MPI_ASSERT(lastInd != numeric_limits<size_t>::max());
    size_t ind = 0;
    vector<string> msgs;
    string nodes;
    string tmpRecSendNodes = "";
    do {
        if(orderIndex >= orders.size()) {
            int rank;
            MPI_Comm_rank(MPI_COMM_WORLD, &rank);
            DEBUG("order_index went beyong the size of orders, rank: %d, lastInd: %lu\n", 
                    rank, lastInd);
            MPI_ASSERT(false);
        }
        msgs = parse(orders[orderIndex++], msgDelimiter);

        /*
         * if we have send nodes at last in a call such as MPI_Test or Wait,
         * we should write to recSendNodes
         */
        if(mpiCallsWithSendNodesAtLast.find(msgs[0]) != mpiCallsWithSendNodesAtLast.end()
                && 
                (msgs[0] != "MPI_Test" 
                 || msgs[3] == "SUCCESS")) {
            tmpRecSendNodes = msgs.back();
            msgs.pop_back();
        } 
        ind = stoul(msgs.back());
        /* if(lastInd > ind) { */
        /*     int rank; */
        /*     MPI_Comm_rank(MPI_COMM_WORLD, &rank); */
        /*     DEBUG("we looping at rank: %d, lastInd %lu, ind %lu\n", rank, lastInd, ind); */
        /* } */
    } while(lastInd > ind);
    if(tmpRecSendNodes.size() > 0
            && recSendNodes != nullptr) {
        *recSendNodes = tmpRecSendNodes;
    }
    return msgs;
}

void appendRecordTracesRaw(vector<string> rawRecordTrace) {
    recordTracesRaw.push_back(rawRecordTrace);
}

size_t getIndex(shared_ptr<element>& eptr) {
    while(eptr->isLoop) {
        eptr = eptr->funcs.back().front();
    }
    return eptr->index; 
}

/*
 * this is for debugging and we are pushing the bbs with the index
 */
vector<shared_ptr<element>> getCurrNodesByIndex(unsigned long index) {
    vector<shared_ptr<element>> vec;
    vector<shared_ptr<element>>& currLevel = recordTraces;
    unsigned long currIndex = 0, nextIndex = 0;
    bool pushed = false;
    while(true) {
        pushed = false;
        for(unsigned i = 0; i < currLevel.size() - 1; i++) {
            currIndex = getIndex(currLevel[i]);
            nextIndex = getIndex(currLevel[i + 1]);
            if(currIndex <= index && index < nextIndex) {
                vec.push_back(currLevel[i]);
                pushed = true;
            }
        } 
        if(pushed == false) {
            currIndex = getIndex(currLevel.back());
            MPI_ASSERT(currIndex <= index);
            vec.push_back(currLevel.back());
        }
        /*
         * here we found the exact point to stop
         */
        if(currIndex == index) {
            break;
        }

        if(vec.back()->funcs.size() == 0) {
            break;
        }
        /* 
         * you need to get the correct currLevel
         */
        pushed = false;
        for(unsigned j = 0; j < vec.back()->funcs.size() - 1; j++) {
            currIndex = getIndex(vec.back()->funcs[j].front());
            nextIndex = getIndex(vec.back()->funcs[j + 1].front());
            if(currIndex <= index && index < nextIndex) {
                currLevel = vec.back()->funcs[j];
                pushed = true;
                break;
            }
        }
        if(pushed == false) {
            currIndex = getIndex(vec.back()->funcs.back().front());
            MPI_ASSERT(currIndex <= index);
            currLevel = vec.back()->funcs.back();
        }
    }
    return vec;

}

/*
 * this is not working for some reason
 * it's looking into main and it's coming back to main, 
 * which makes no sense
 */
size_t getIndexFromDeque(deque<shared_ptr<lastaligned>>& q) {
    shared_ptr<element> eptr = nullptr;
    auto currTrace = recordTraces;
    cerr << "recordTraces.size(): " << recordTraces.size() << endl;
    for(auto i = q.size() - 1; 0 <= i; i--) {
        auto origIndex = q[i]->origIndex; 
        auto funcId = q[i]->funcId;
        MPI_ASSERT(origIndex != numeric_limits<size_t>::max());
        cerr << *currTrace[origIndex] << endl;
        MPI_ASSERT(origIndex < currTrace.size());
        if(funcId != numeric_limits<size_t>::max()) {
            funcId = 0;
            currTrace = currTrace[origIndex]->funcs[funcId]; 
            cerr << "currTrace.size(): " << currTrace.size() << endl;
        } else {
            eptr = currTrace[origIndex];
            break;
        }
    }
    if(eptr == nullptr) {
        return -1;
    }
    return eptr->index;
}
