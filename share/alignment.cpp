
#include "alignment.h"

using namespace std;

/* static FILE* replayTraceFile = nullptr; */
// recordTracesRaw is simply a local variable, so don't make it a global variable
vector<shared_ptr<element>> recordTraces;

vector<vector<string>> replayTracesRaw; // this is necessary for bbprinter
vector<shared_ptr<element>> replayTraces;
static unordered_map<string, unordered_set<string>> divs;

vector<shared_ptr<element>> __makeHierarchyLoop(vector<vector<string>>& traces, unsigned long& index, unordered_map<string, loopNode *>& loopTrees, loopNode *currloop);
/* element::element(bool isEntry, bool isExit, string& bb) : isEntry(isEntry), isExit(isExit), bb(bb), funcs(vector<vector<shared_ptr<element>>>()) { */
/* } */

element::element(bool isEntry, bool isExit, int id, string& funcname, bool isLoop) : \
        funcs(vector<vector<shared_ptr<element>>>()), funcname(funcname), index(numeric_limits<unsigned long>::max()), id(id), isEntry(isEntry), isExit(isExit), isLoop(isLoop) {
}

element::element(bool isEntry, bool isExit, int id, string& funcname, unsigned long index, bool isLoop) : \
        funcs(vector<vector<shared_ptr<element>>>()), funcname(funcname), index(index), id(id), isEntry(isEntry), isExit(isExit), isLoop(isLoop){
            /* DEBUG0("element created with index: %lu\n", index); */
}

element::element(int id, string& funcname, unsigned long index, bool isLoop) : \
        funcs(vector<vector<shared_ptr<element>>>()), funcname(funcname), index(index), id(id), isEntry(false), isExit(false), isLoop(isLoop) {
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

// this does not print whether the node is a loop node or not
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
    return (isEntry == e.isEntry && isExit == e.isExit && id == e.id && funcname == e.funcname && isLoop == e.isLoop);
}

lastaligned::lastaligned() : funcId(numeric_limits<unsigned long>::max()), origIndex(numeric_limits<unsigned long>::max()), repIndex(numeric_limits<unsigned long>::max()) {
}

lastaligned::lastaligned(unsigned long funcId, unsigned long origIndex, unsigned long repIndex) : funcId(funcId), origIndex(origIndex), repIndex(repIndex) {
}

bool lastaligned::isSuccess() {
    return origIndex == numeric_limits<unsigned long>::max() && repIndex == numeric_limits<unsigned long>::max();
}

ostream& operator<<(ostream& os, const lastaligned& l) {
    os << "funcId: " << l.funcId << ", origIndex: " << l.origIndex << ", repIndex: " << l.repIndex;
    return os;
}

vector<shared_ptr<element>> makeHierarchyMain(vector<vector<string>>& traces, unsigned long& index) {
    vector<shared_ptr<element>> functionalTraces;
    bool isEntry, isExit;
    string funcname = "main";
    string bbname;
    while(index < traces.size() && funcname != traces[index][0]) {
        index++;
    }

    if(index >= traces.size()) {
        DEBUG("index: %lu, traces.size(): %lu\n", index, traces.size()); 
        return functionalTraces;
    }

    do {
        MPI_ASSERT(traces[index][0] == funcname);
        MPI_ASSERT(traces[index].size() == 3 || traces[index].size() == 4);
        bbname = traces[index][0] + ":" + traces[index][1] + ":" + traces[index][2]; 
        isEntry = (traces[index][1] == "entry");
        isExit = (traces[index][1] == "exit");
        /* DEBUG0("bbname: %s\n", bbname.c_str()); */
        shared_ptr<element> eptr;
        if(traces[index].size() == 4) {
            eptr = make_shared<element>(isEntry, isExit, stoi(traces[index][2]), traces[index][0], stoul(traces[index][3]));
        } else {
            eptr = make_shared<element>(isEntry, isExit, stoi(traces[index][2]), traces[index][0]);
        }
        index++;
        
        while(!isExit && index < traces.size() && traces[index][1] == "entry") {
            eptr->funcs.push_back(makeHierarchy(traces, index));
        }
        functionalTraces.push_back(eptr);
    } while(!isExit && index < traces.size());

    return functionalTraces;
}

static loopNode *isNewLoop(string bbname, loopNode *ln) {
    MPI_ASSERT(ln != nullptr);
    /* DEBUG0("isNewLoop with bbname: %s, at ln of %s\n", bbname.c_str(), ln->entry.c_str()); */
    for(auto &c: ln->children) {
        if(c->nodes.find(bbname) != c->nodes.end()) {
            return c;
        }
    }
    return nullptr;
}

vector<shared_ptr<element>> makeHierarchyLoop(vector<vector<string>>& traces, unsigned long& index, unordered_map<string, loopNode *>& loopTrees)  {
    vector<shared_ptr<element>> functionalTraces;
    shared_ptr<element> eptr;
    bool isEntry = false, isExit = false;
    string bbname, funcname = traces[index][0];
    loopNode *currloop = nullptr, *child = nullptr;
    int iterationcnt = 1;

    // here we are getting the entry 
    MPI_ASSERT(traces[index][1] == "entry");
    currloop = loopTrees[funcname];

    while (!isExit && index < traces.size()) {
        MPI_ASSERT(index < traces.size());
        bbname = traces[index][0] + ":" + traces[index][1] + ":" + traces[index][2];
        MPI_ASSERT(traces[index][0] == funcname);
        MPI_ASSERT(traces[index].size() == 3 || traces[index].size() == 4);
        isEntry = (traces[index][1] == "entry");
        isExit = (traces[index][1] == "exit");

        // case 1: if we are in the inner loop, recursively call the function
        if((child = isNewLoop(bbname, currloop)) != nullptr) {
            vector<shared_ptr<element>> loops = __makeHierarchyLoop(traces, index, loopTrees, child);
            functionalTraces.insert(functionalTraces.end(), loops.begin(), loops.end());
        } else {       
            if(traces[index].size() == 4) {
                eptr = make_shared<element>(isEntry, isExit, stoi(traces[index][2]), traces[index][0], stoul(traces[index][3]));
            } else {
                eptr = make_shared<element>(isEntry, isExit, stoi(traces[index][2]), traces[index][0]);
            }
            index++;
            while(!isExit && index < traces.size() && traces[index][1] == "entry") {

                eptr->funcs.push_back(makeHierarchyLoop(traces, index, loopTrees)); 
            }
            functionalTraces.push_back(eptr);
        }
    } 

    return functionalTraces; 
}


vector<shared_ptr<element>> __makeHierarchyLoop(vector<vector<string>>& traces, unsigned long& index, unordered_map<string, loopNode *>& loopTrees, loopNode *currloop) {
    vector<shared_ptr<element>> iterations, curriter;
    shared_ptr<element> eptr;
    bool isEntry = false, isExit = false;
    string bbname, funcname = traces[index][0];
    loopNode *child = nullptr;
    int iterationcnt = 1;

    // here we are getting the entry 
    vector<string> entryinfo = parse(currloop->entry, ':');
    MPI_ASSERT(entryinfo.size() == 3);
    MPI_ASSERT(entryinfo[0] == funcname);
    int entryindex = stoi(entryinfo[2]);

    while (!isExit && index < traces.size()) {
        MPI_ASSERT(index < traces.size());
        bbname = traces[index][0] + ":" + traces[index][1] + ":" + traces[index][2];
        MPI_ASSERT(traces[index][0] == funcname);
        MPI_ASSERT(traces[index].size() == 3 || traces[index].size() == 4);
        isEntry = (traces[index][1] == "entry");
        isExit = (traces[index][1] == "exit");

        // case 1: if we are in the inner loop, recursively call the function
        if((child = isNewLoop(bbname, currloop)) != nullptr) {
            /* DEBUG0("printing bbname: %s\n", bbname.c_str()); */
            vector<shared_ptr<element>> loops = __makeHierarchyLoop(traces, index, loopTrees, child);
            curriter.insert(curriter.end(), loops.begin(), loops.end());
            continue;
        
        // case 2: if we are not in the current loop, we should return what we have so far
        } else if(currloop->nodes.find(bbname) == currloop->nodes.end()) {
            eptr = make_shared<element>(entryindex, funcname, iterationcnt++); 
            eptr->funcs.push_back(curriter);
            curriter.clear();
            iterations.push_back(eptr);
            return iterations;

        // case 3: we have gone through the back edge
        } else if(curriter.size() > 0 && bbname == currloop->entry) {
            eptr = make_shared<element>(entryindex, funcname, iterationcnt++);
            eptr->funcs.push_back(curriter);
            curriter.clear();
            iterations.push_back(eptr);
        }
        // else
        if(traces[index].size() == 4) {
            eptr = make_shared<element>(isEntry, isExit, stoi(traces[index][2]), traces[index][0], stoul(traces[index][3]));
        } else {
            eptr = make_shared<element>(isEntry, isExit, stoi(traces[index][2]), traces[index][0]);
        }
        curriter.push_back(eptr);
        index++;
        if(index >= traces.size()) {
            break;
        }

        while(!isExit && index < traces.size() && traces[index][1] == "entry") {

            eptr->funcs.push_back(makeHierarchyLoop(traces, index, loopTrees)); 
        }
    } 
    MPI_ASSERT(!isExit);

    // we could be here because we have reached the end of the trace
    eptr = make_shared<element>(entryindex, funcname, iterationcnt++);
    eptr->funcs.push_back(curriter);
    iterations.push_back(eptr);
    
    return iterations; 
}

vector<shared_ptr<element>> makeHierarchyMain(vector<vector<string>>& traces, unsigned long &index, unordered_map<string, loopNode *>& loopTrees) {
    vector<shared_ptr<element>> functionalTraces;
    bool isEntry = false, isExit = false;
    string funcname = "main";
    MPI_ASSERT(loopTrees.find(funcname) != loopTrees.end());
    loopNode *loopTree = loopTrees[funcname], *child = nullptr;
    string bbname;
    while(index < traces.size() && funcname != traces[index][0]) {
        index++;
    }
    while(!isExit && index < traces.size()) {
        MPI_ASSERT(traces[index][0] == funcname);
        MPI_ASSERT(traces[index].size() == 3 || traces[index].size() == 4);
        bbname = traces[index][0] + ":" + traces[index][1] + ":" + traces[index][2];

        // this returns the ptr of a child that has bbname in it
        // if none, then it returns nullptr
        if((child = isNewLoop(bbname, loopTree)) != nullptr) {
            vector<shared_ptr<element>> loops = __makeHierarchyLoop(traces, index, loopTrees, child);
            functionalTraces.insert(functionalTraces.end(), loops.begin(), loops.end());
            continue;
        }
        isEntry = (traces[index][1] == "entry");
        isExit = (traces[index][1] == "exit");
        shared_ptr<element> eptr;
        if(traces[index].size() == 4) {
            eptr = make_shared<element>(isEntry, isExit, stoi(traces[index][2]), traces[index][0], stoul(traces[index][3]));
        } else {
            eptr = make_shared<element>(isEntry, isExit, stoi(traces[index][2]), traces[index][0]);
        }
        index++;

        while(!isExit && index < traces.size() && traces[index][1] == "entry") {
            eptr->funcs.push_back(makeHierarchyLoop(traces, index, loopTrees));
        }
        functionalTraces.push_back(eptr);
    }
    return functionalTraces;
    
}

/*  1. Every function starts from entry except for main */
/*  2. Let's not take anything in unless the main function has started */
/*  3. Let's not take anything in after the main function has ended */
/*  4. Even if the function name is the same, if we reach the new entry node, we recursively call the function */
vector<shared_ptr<element>> makeHierarchy(vector<vector<string>>& traces, unsigned long& index) {
    vector<shared_ptr<element>> functionalTraces;
    bool isEntry, isExit;
    string bbname, funcname;
    funcname = traces[index][0];
    /* bool debug = false; */
    if(funcname == "hypre_PCGSolve") {
        DEBUG0("came to hypre_PCGSolve at makeHierarchy\n");
        MPI_ASSERT(false);
    }
    /* DEBUG0("makeHierarchy for %s, at index: %lu\n", funcname.c_str(), index); */
    /* DEBUG0("funcname: %s: %lu\n", funcname.c_str(), index); */
    do {
        MPI_ASSERT(index < traces.size());
        bbname = traces[index][0] + ":" + traces[index][1] + ":" + traces[index][2];
        /* if(bbname == "hypre_PCGSolve:neither:15") { */
        /*     DEBUG0("came to hypre_PCGSolve:neither:15\n"); */
        /*     DEBUG0("show me next %s:%s:%s\n", traces[index + 1][0].c_str(), traces[index + 1][1].c_str(), traces[index + 1][2].c_str()); */
        /*     debug = true; */
        /* } */
        /* if(traces[index][0] != funcname) { */
        /*     DEBUG0("bbname: %s\n", bbname.c_str()); */
        /*     DEBUG0("funcname: %s, traces[index][0]: %s\n", funcname.c_str(), traces[index][0].c_str()); */
        /* } */
        MPI_ASSERT(traces[index][0] == funcname);
        MPI_ASSERT(traces[index].size() == 3 || traces[index].size() == 4);
        /* DEBUG0("bbname: %s\n", bbname.c_str()); */
        isEntry = (traces[index][1] == "entry");
        isExit = (traces[index][1] == "exit");
        /* shared_ptr<element> eptr = make_shared<element>(isEntry, isExit, bbname); */
        shared_ptr<element> eptr;
        if(traces[index].size() == 4) {
            eptr = make_shared<element>(isEntry, isExit, stoi(traces[index][2]), traces[index][0], stoul(traces[index][3]));
        } else {
            eptr = make_shared<element>(isEntry, isExit, stoi(traces[index][2]), traces[index][0]);
        }
        index++;
        while(!isExit && index < traces.size() && traces[index][1] == "entry") {
            eptr->funcs.push_back(makeHierarchy(traces, index)); 
            /* if(debug) { */
            /*     int rank; */
            /*     MPI_Comm_rank(MPI_COMM_WORLD, &rank); */
            /*     if(!rank) { */
                /* print(eptr->funcs.back(), 0); */
                /* DEBUG0("eptr->bb(): %s\n", eptr->bb().c_str()); */
            /*     debug = false; */
            /*     } */
            /* } */
        }
        functionalTraces.push_back(eptr);
    } while(!isExit && index < traces.size());

    return functionalTraces; 
}

static inline bool isLoopEntry(string bbname, shared_ptr<element> parent, loopNode *currloop) {
    string parentbb;     
    if(!parent) return false;
    if(parent->isEntry) {
        MPI_ASSERT(!parent->isExit);
        parentbb = parent->funcname + ":entry:" + to_string(parent->id);
    } else if(parent->isExit) {
        parentbb = parent->funcname + ":exit:" + to_string(parent->id);
    } else {
        parentbb = parent->funcname + ":neither:" + to_string(parent->id);
    }
    return parent->isLoop & parentbb == bbname && currloop->nodes.size() > 0 && bbname == currloop->entry;
}

void addHierarchy(vector<shared_ptr<element>>& functionalTraces, vector<vector<string>>& traces, unsigned long& index, unordered_map<string, loopNode *>& loopTrees) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    stack<shared_ptr<element>> __stack;
    MPI_ASSERT(!functionalTraces.empty());
    MPI_ASSERT(functionalTraces[0]->funcname == "main");
    MPI_ASSERT(functionalTraces.back()->funcname == "main");
    shared_ptr<element> curr = functionalTraces.back();
    loopNode *currloop = nullptr, *tmploop = nullptr;

    /* DEBUG0("traces.size(): %lu\n", traces.size()); */
    while(!(curr->isExit)) {
        __stack.push(curr);
        //DEBUG0("curr->bb(): %s in the stack\n", curr->bb().c_str());
        if(curr->funcs.size() == 0) {
            break;
        } else {
            curr = curr->funcs.back().back();
        }
    }

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

    /* DEBUG0("option 1\n"); */
    // initialize the correct currloop
    currloop = loopTrees[curr->funcname];
    while((tmploop = isNewLoop(curr->bb(), currloop)) != nullptr) {
        currloop = tmploop;
    }

    /* DEBUG0("option 2\n"); */
    while(index < traces.size()) {
        // if we are at the entry, we would go through this
        while(index < traces.size() && traces[index][1] == "entry") {
            curr->funcs.push_back(makeHierarchyLoop(traces, index, loopTrees));
        }
        if(index >= traces.size()) break;
        MPI_ASSERT(index < traces.size());
        MPI_ASSERT(traces[index].size() > 0);
        //bool flag = false;
        string bb = traces[index][0] + ":" + traces[index][1] + ":" + traces[index][2]; 
        //DEBUG0("option 3, curr->bb: %s, bb:%s, currloop->entry: %s\n", curr->bb().c_str(), bb.c_str(), currloop->entry.c_str());
        /* if(parent) { */
            /* DEBUG0("parent->bb(): %s, parent->bb == bb: %d \n", parent->bb().c_str(), parent->bb() == bb); */
        /* } */
        
        
        // we need to think of two cases
        // 1. we are already in the loop
        // 2. we are newly in the loop
        if(isLoopEntry(bb, parent, currloop)) {
            // implement the case where we have to go through the current loop
            /* if(parent->funcname != curr->funcname) { */
                /* cerr << parent->bb() << " " << curr->bb() << endl; */
                /* print(replayTraces, 0); */
            /* } */
            MPI_ASSERT(parent->funcname == curr->funcname);
            MPI_ASSERT(parent->isLoop);
            vector<shared_ptr<element>> iterations = __makeHierarchyLoop(traces, index, loopTrees, currloop);
            if(!__stack.empty()) {
                __stack.top()->funcs.back().insert(__stack.top()->funcs.back().end(), iterations.begin(), iterations.end());
                curr = parent;
                parent = __stack.top();
                __stack.pop();
            } else {
                functionalTraces.insert(functionalTraces.end(), iterations.begin(), iterations.end());
                curr = parent;
                parent = nullptr;
            }

            MPI_ASSERT(currloop->nodes.size() > 0 && currloop->parent);
            currloop = currloop->parent;
        }

        if(index >= traces.size()) break;

        bb = traces[index][0] + ":" + traces[index][1] + ":" + traces[index][2];
        //if(bb == "hypre_StructGridAssembleWithAP:neither:69") {
        //    DEBUG0("hypre_StructGridAssembleWithAP:neither:69\n");
        //    DEBUG0("currloop->entry: %s\n", currloop->entry.c_str());
        //}

        //DEBUG0("option 4\n"); 
        if(rank == 0 && currloop->nodes.size() == 0 && loopTrees[curr->funcname] != currloop) {
            cerr << curr->funcname << endl \
                << currloop->entry << endl;
        }
        MPI_ASSERT(currloop->nodes.size() > 0 || loopTrees[curr->funcname] == currloop);
        while((tmploop = isNewLoop(bb, currloop)) != nullptr) {
            // you need to make an loop element here
            //DEBUG0("option 4.1\n"); 
            vector<shared_ptr<element>> loops = __makeHierarchyLoop(traces, index, loopTrees, tmploop);
            if(parent != nullptr) {
                parent->funcs.back().insert(parent->funcs.back().end(), loops.begin(), loops.end());
            } else {
                functionalTraces.insert(functionalTraces.end(), loops.begin(), loops.end());
            }
            /* DEBUG0("option 4.2\n"); */
            if(index >= traces.size()) break;
            bb = traces[index][0] + ":" + traces[index][1] + ":" + traces[index][2];
        }

        if(index >= traces.size()) break;
        //DEBUG0("option 5\n");  
        MPI_ASSERT(traces[index][1] != "entry");
        // check if we are getting out of this loop
        if(currloop->nodes.size() > 0 && currloop->nodes.find(bb) == currloop->nodes.end()) {
            MPI_ASSERT(parent != nullptr); 
            MPI_ASSERT(parent->isLoop);
            while(currloop->nodes.size() > 0 && currloop->nodes.find(bb) == currloop->nodes.end()) {
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
        }

        if(traces[index].size() == 3) {
            newchild = make_shared<element>(traces[index][1] == "entry", traces[index][1] == "exit", stoi(traces[index][2]), traces[index][0]);
        } else {
            MPI_ASSERT(traces[index].size() == 4);
            newchild = make_shared<element>(traces[index][1] == "entry", traces[index][1] == "exit", stoi(traces[index][2]), traces[index][0], stoul(traces[index][3]));
        }
        MPI_ASSERT(traces[index][0] == curr->funcname);
        index++;
    
        /* DEBUG0("option 6\n"); */
        if(parent != nullptr) {
            /* DEBUG0("option 6.1\n"); */
            if(parent->funcs.size() == 0) {
                parent->funcs.push_back(vector<shared_ptr<element>>());
            }
            parent->funcs.back().push_back(newchild);
        } else {
            /* DEBUG0("option 6.2\n"); */
            functionalTraces.push_back(newchild);
        }
        
        /* DEBUG0("option 7\n"); */
        // if we are ending this batch, it should be either a loop iteration, or a function
        // just add a condition || and add the condition of the end of the loop
        if(index >= traces.size()) {
            break;
        }
     
        bb = traces[index][0] + ":" + traces[index][1] + ":" + traces[index][2];
        //DEBUG0("option 8 bb: %s, currloop->entry: %s\n", bb.c_str(), currloop->entry.c_str());

        if(traces[index - 1][1] == "exit") {
            //DEBUG0("option 8.1: finding an exit here\n"); 
            if(parent == nullptr) {
                MPI_ASSERT(traces[index - 1][1] == "exit");
                break;
            } else {
                // this is just trying to be careful, should omit in release
                if(traces[index - 1][1] != "exit" && currloop->entry == bb) {
                    MPI_ASSERT(curr->funcname == parent->funcname);
                }
                curr = parent;
                if(!__stack.empty()) {
                    parent = __stack.top();
                    __stack.pop();
                } else {
                    parent = nullptr;
                }
            }
            // need to update the looptrees as well -> this needs to be rethought
            currloop = loopTrees[curr->funcname];
            while((tmploop = isNewLoop(curr->content(), currloop)) != nullptr) {
                currloop = tmploop;
            }
            if(currloop->entry == bb) {
                cerr << "we are facing this situation now: " << bb << endl;
                // the situation is... 
                // 1. the last node was an exit bb of the function
                // 2. and now the current trace is an entry of the loop
                // if the parent is the virtual node of this loop, you need to make another virtual node for this iterations
                // if not, you stil need to make another virtual node
                MPI_ASSERT(false);
            }
        } else {
            /* DEBUG0("option 8.4\n"); */
            curr = newchild;
        }
    }
    /* DEBUG0("option 9\n"); */
}

/* void addHierarchy(vector<shared_ptr<element>>& functionalTraces, vector<vector<string>>& traces, unsigned long& index) { */
/*     int rank; */
/*     MPI_Comm_rank(MPI_COMM_WORLD, &rank); */
/*     stack<shared_ptr<element>> __stack; */
/*     MPI_ASSERT(!functionalTraces.empty()); */
/*     MPI_ASSERT(functionalTraces[0]->funcname == "main"); */
/*     MPI_ASSERT(functionalTraces.back()->funcname == "main"); */
/*     shared_ptr<element> curr = functionalTraces.back(); */

/*     while(!(curr->isExit)) { */
/*         __stack.push(curr); */
/*         if(curr->funcs.size() == 0) { */
/*             break; */
/*         } else { */
/*             curr = curr->funcs.back().back(); */
/*         } */
/*     } */
/*     shared_ptr<element> parent, newchild; */
/*     MPI_ASSERT(!__stack.empty()); */
/*     curr = __stack.top(); */
/*     __stack.pop(); */
/*     if(__stack.empty()) { */
/*         parent = nullptr; */
/*     } else { */
/*         parent = __stack.top(); */
/*         __stack.pop(); */
/*     } */

/*     while(index < traces.size()) { */
/*         while (index < traces.size() && traces[index][1] == "entry") { */
/*             curr->funcs.push_back(makeHierarchy(traces, index)); */
/*         } */ 
/*         if(index >= traces.size()) { */
/*             break; */
/*         } */ 
/*         MPI_ASSERT(traces[index][1] != "entry"); */
/*         /1* DEBUG0("%s:%s:%s\n", traces[index][0].c_str(), traces[index][1].c_str(), traces[index][2].c_str()); *1/ */
/*         if(traces[index].size() == 3) { */
/*             newchild = make_shared<element>(traces[index][1] == "entry", traces[index][1] == "exit", stoi(traces[index][2]), traces[index][0]); */
/*         } else { */
/*             newchild = make_shared<element>(traces[index][1] == "entry", traces[index][1] == "exit", stoi(traces[index][2]), traces[index][0], stoul(traces[index][3])); */
/*         } */
/*         MPI_ASSERT(traces[index][0] == curr->funcname); */
/*         index++; */
/*         if(parent != nullptr) { */
/*             parent->funcs.back().push_back(newchild); */
/*         } else { */
/*             functionalTraces.push_back(newchild); */
/*         } */
/*         if(traces[index - 1][1] == "exit") { // this index - 1 is the last index */
/*             if(parent == nullptr) { */
/*                 break; // this should be at the end of main */
/*             } else { */
/*                 curr = parent; */
/*                 if(!__stack.empty()) { */
/*                     parent = __stack.top(); */
/*                     __stack.pop(); */
/*                 } else { */
/*                     parent = nullptr; */
/*                 } */
/*             } */
/*         } else { */
/*             curr = newchild; */
/*         } */
/*     } */
/* } */

void print(vector<shared_ptr<element>>& functionalTraces, unsigned depth) {
    for(auto& eptr : functionalTraces) {
        for(unsigned i = 0; i < depth; i++) {
            fprintf(stderr, "\t"); 
        }
        if(eptr->index != numeric_limits<unsigned long>::max()) {
            if(eptr->isLoop) 
                fprintf(stderr, "%s\n", eptr->bb().c_str());
            else
                fprintf(stderr, "%s:%lu\n", eptr->bb().c_str(), eptr->index);

        } else {
            fprintf(stderr, "%s\n", eptr->bb().c_str());
        }

        for(unsigned i = 0; i < eptr->funcs.size(); i++) {
            print(eptr->funcs[i], depth + 1);
        }
    }
}

void printsurface(vector<shared_ptr<element>>& functionalTraces) {
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

/* deque<shared_ptr<lastaligned>> onlineAlignment(deque<shared_ptr<lastaligned>>& q, bool& isaligned, size_t &lastind) { */
/*     cerr << "this function should not be called\n"; */
/*     MPI_ASSERT(false); */
/*     int rank; */
/*     MPI_Comm_rank(MPI_COMM_WORLD, &rank); */
/*     /1* static deque<shared_ptr<lastaligned>> q; *1/ */
/*     /1* DEBUG("onlineAlignment called at rank: %d\n", rank); *1/ */
/*     appendReplayTrace(); */
/*     size_t i = 0, j = 0; */ 
/*     size_t funcId = 0; */
/*     if(!q.empty()) { */
/*         i = q.back()->origIndex; */
/*         j = q.back()->repIndex; */
/*         funcId = q.back()->funcId; */
/*         q.pop_back(); */
/*     } */
/*     /1* DEBUG0("online alignment starting\n"); *1/ */
/*     deque<shared_ptr<lastaligned>> rq; */
/*     rq = greedyalignmentOnline(recordTraces, replayTraces, q, i, j, funcId, rank, isaligned, lastind); */
/*     MPI_ASSERT(lastind != numeric_limits<size_t>::max()); */
/*     /1* DEBUG0("lastind: %lu at %d\n", lastind, __LINE__); *1/ */
/*     /1* if(rank == 0) { *1/ */
/*         /1* cerr << "printing rq at the end\n" << rq << endl; *1/ */
/*     /1* } *1/ */
/*     /1* DEBUG("online alignment done at rank: %d\n", rank); *1/ */
/*     return rq; */
/* } */

deque<shared_ptr<lastaligned>> onlineAlignment(deque<shared_ptr<lastaligned>>& q, bool& isaligned, size_t &lastind, unordered_map<string, loopNode *>& loopTrees) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    /* DEBUG("onlineAlignment called at rank: %d\n", rank); */
    appendReplayTrace(loopTrees);
    /* print(replayTraces, 0); */
    
    size_t i = 0, j = 0; 
    size_t funcId = 0; 
    /* if(rank ==  0) { */
        /* shared_ptr<element> recelement, repelement; */
        /* DEBUG("q.size(): %lu\n", q.size()); */
        /* for(unsigned i = 0; i < q.size(); i++) { */
            /* DEBUG("origIndex: %lu, repIndex: %lu, funcId: %lu\n", q[i]->origIndex, q[i]->repIndex, q[i]->funcId); */
            //recelement = recordTraces[q[i]->origIndex];
            //repelement = replayTraces[q[i]->repIndex];
            //cerr << recelement->bb() << " " << repelement->bb() << " " << q[i]->funcId << endl; 
        /* } */
    /* } */
    /* DEBUG0("option 10\n"); */
    if(!q.empty()) { 
         i = q.back()->origIndex; 
         j = q.back()->repIndex; 
         funcId = q.back()->funcId; 
         q.pop_back(); 
    } 
    deque<shared_ptr<lastaligned>> rq;
    isaligned = true; 
    
    rq = greedyalignmentOnline(recordTraces, replayTraces, q, i, j, funcId, rank, isaligned, lastind); 
    /* DEBUG0("option 11\n"); */
    MPI_ASSERT(lastind != numeric_limits<size_t>::max()); 
    /* DEBUG0("lastind: %lu at %d\n", lastind, __LINE__); */
    //if(rank == 0) { 
    //    cerr << "printing rq at the end\n" << rq << endl; 
    //} 
    /* DEBUG("online alignment done at rank: %d\n", rank); */
    return rq;
}

/* void appendReplayTrace() { */
/*     int rank; */
/*     MPI_Comm_rank(MPI_COMM_WORLD, &rank); */
/*     static unsigned long index = 0; */
/*     if(replayTraces.size() == 0) { */
/*         /1* DEBUG0("makeHierarchyMain called at %lu\n", index); *1/ */
/*         MPI_ASSERT(index == 0); */
/*         replayTraces = makeHierarchyMain(replayTracesRaw, index); */
/*         /1* if(rank == 0) { *1/ */
/*             /1* print(replayTraces, 0); *1/ */
/*         /1* } *1/ */
/*     } else { */
/*         /1* DEBUG0("addHierarchy called at %lu\n", index); *1/ */
/*         addHierarchy(replayTraces, replayTracesRaw, index); */
/*         /1* if(rank == 0) { *1/ */
/*         /1*     print(replayTraces, 0); *1/ */
/*         /1* } *1/ */
/*     } */

/*     return; */
/* } */

void appendReplayTrace(unordered_map<string, loopNode *>& loopTrees) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    static unsigned long index = 0;
    /* DEBUG0("index at %lu\n", index); */
    if(replayTraces.size() == 0) {
        /* DEBUG0("makeHierarchyMain called at %lu\n", index); */
        MPI_ASSERT(index == 0);
        replayTraces = makeHierarchyMain(replayTracesRaw, index, loopTrees);
        /* if(rank == 0) { */
            /* print(replayTraces, 0); */
        /* } */
    } else {
        /* DEBUG0("addHierarchy called at %lu\n", index); */
        addHierarchy(replayTraces, replayTracesRaw, index, loopTrees);
        /* DEBUG0("returning append replay trace\n"); */
        /* if(rank == 0) { */
        /*     print(replayTraces, 0); */
        /* } */
    }

    return;
}

static inline void create_map(unordered_map<string, vector<size_t>>& bbMap, vector<shared_ptr<element>>& reproduced, size_t& j) {
    for(unsigned k = j; k < reproduced.size(); k++) {
        if(bbMap.find(reproduced[k]->bb()) == bbMap.end()) {
            bbMap[reproduced[k]->bb()] = vector<size_t>();
        }
        bbMap[reproduced[k]->bb()].push_back(k);
    }
    return;
}

static inline bool matchbbmap(unordered_map<string, vector<size_t>>& bbMap, vector<shared_ptr<element>>& original, size_t& i, size_t& j) {
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


static pair<size_t, size_t> __greedyalignmentOnline(vector<shared_ptr<element>>& original, vector<shared_ptr<element>>& reproduced, vector<pair<shared_ptr<element>, shared_ptr<element>>>& aligned, size_t& initi, size_t& initj, const int& rank, bool&isaligned, size_t& lastind) {
    /* DEBUG0("__greedyalignmentOnline at i: %lu, j: %lu, ", \ */
            /* initi, initj); */
    /* DEBUG0("__greedyalignmentOnline at %s, i: %lu, j: %lu, ", \ */
            /* original[initi]->bb().c_str(), initi, initj); */
    size_t i = initi, j = initj;
    unordered_map<string, vector<size_t>> bbMap;
    while(i < original.size() && j < reproduced.size()) {
        if(original[i]->bb() == reproduced[j]->bb()) {
            aligned.push_back(make_pair(original[i], reproduced[j]));
            i++; j++;
        } else {
            /* if(i != initi || j != initj) { */
            printf("pod: %s, rank: %d\n", original[i - 1]->bb().c_str(), rank);
            if(rank == 0) {
                cerr << "replay\n";
                printsurface(reproduced);
                cerr << "replay2\n";
                printsurface(reproduced[1]->funcs[0]);
                
                cerr << "record\n"; 
                printsurface(original);
                /* print(replayTraces, 0); */
                /* cerr << "record\n"; */ 
                /* print(recordTraces, 0); */
            }
            MPI_ASSERT(false);
            /* if(original[i - 1]->bb() == reproduced[j - 1]->bb()) { */
                /* string bb = original[i - 1]->bb(); */
                /* if(rank == 0) { */
                /* for(unsigned i = 0; i < reproduced.size(); i++) { */
                    /* DEBUG0("reproduced[%u]: %s\n", i, reproduced[i]->bb().c_str()); */    
                /* } */ 
                /* } */
                /* printf("pod: %s, rank: %d\n", bb.c_str(), rank); */
            /* } */
            /* if(divs.find(bb) == divs.end()) { */
                /* divs[original[i - 1]->bb()] = unordered_set<string>(); */
            /* } */
            /* } */
            size_t oldj = j, oldi = i;
            if(bbMap.size() == 0) create_map(bbMap, reproduced, j);
            if(!matchbbmap(bbMap, original, i, j)) {
                // never aligned here
                lastind = original[oldi]->index;
                /* DEBUG0("at __greedyalignment lastind updated: %lu\n", lastind); */
                MPI_ASSERT(lastind != numeric_limits<size_t>::max());
                /* DEBUG0("never aligned at i: %lu, j: %lu, from %s\n", oldi, oldj, original[oldi - 1]->bb().c_str()); */
                /* if(rank == 0) { */
                    /* DEBUG0("original\n"); */
                    /* printsurface(original); */
                    /* DEBUG0("reproduced\n"); */
                    /* printsurface(reproduced); */
                /* } */
                isaligned = false;
                return make_pair(oldi, oldj);
            } else {
                /* printf("pod: %s, rank: %d\n", bb.c_str(), rank); */
                printf("poc: %s, rank: %d\n", original[i]->bb().c_str(), rank);
                /* if(divs[bb].find(original[i]->bb()) == divs[bb].end()) { */
                    /* divs[bb] = unordered_set<string>(); */
                /* } */
                /* divs[bb].insert(original[i]->bb()); */
                i++; j++;
            }
        }
    }
    isaligned = true;
    if(i < original.size()) {
        /* DEBUG0("did not finish at i: %lu\n", i - 1); */
        lastind = original[i - 1]->index;
        /* DEBUG0("at __greedyalignment %d lastind updated: %lu\n", rank, lastind); */
        /* if(lastind == numeric_limits<size_t>::max()) { */
            /* print(original, 0); */
            /* print(reproduced, 0); */
        /* } */
        MPI_ASSERT(lastind != numeric_limits<size_t>::max());
        return make_pair(i - 1, j - 1);
    } else {
        /* if(j != reproduced.size()) { */
        /*     if(rank == 0) { */
        /*         cerr << "original\n"; */
        /*         printsurface(original); */
        /*         cerr << "reproduced\n"; */
        /*         printsurface(reproduced); */
        /*         print(replayTraces, 0); */
        /*     } */
        /* } */
        MPI_ASSERT(j == reproduced.size());
        /* DEBUG0("aligned till the end\n"); */
        lastind = original[i - 1]->index;
        /* DEBUG0("at __greedyalignment %d lastind updated: %lu\n", rank, lastind); */
        MPI_ASSERT(lastind != numeric_limits<size_t>::max());
        return make_pair(numeric_limits<size_t>::max(), numeric_limits<size_t>::max());
    }
}

static bool issuccessful(pair<size_t, size_t> p) {
    return p.first == numeric_limits<size_t>::max() && p.second == numeric_limits<size_t>::max();
}

// this function is just to make sure
static bool islastaligned(vector<shared_ptr<element>>& original, vector<shared_ptr<element>>& reproduced, size_t i, size_t j) {
    unordered_map<string, vector<size_t>> bbMap;
    create_map(bbMap, reproduced, j);
    return matchbbmap(bbMap, original, i, j);
}

deque<shared_ptr<lastaligned>> greedyalignmentOnline(vector<shared_ptr<element>>& original, vector<shared_ptr<element>>& reproduced, deque<shared_ptr<lastaligned>>& q, size_t &i, size_t &j, const size_t& funcId, const int &rank, bool& isaligned, size_t& lastind) {
    MPI_ASSERT(original[i]->funcname == reproduced[j]->funcname);
    // if the queue is not empty, let's do the alignment level below first
    deque<shared_ptr<lastaligned>> rq;
    /* DEBUG0("greedyalignmentOnline at %s with i: %lu, j: %lu, funcId:%lu\n",\ */
            /* original[i]->bb().c_str(), i, j, funcId); */

    // in case the q is not empty, we need to do the alignment level below first
    bool firsttime = false;
    /* DEBUG0("option 12\n"); */
    if(!q.empty()) {
        shared_ptr<lastaligned> la = q.back();
        if(la->origIndex == numeric_limits<size_t>::max() && la->repIndex == numeric_limits<size_t>::max()) {
            // so we check if the next one has been completed
            q.clear();
        } else {
            q.pop_back();
            size_t tmpfuncId = la->funcId;
            if(tmpfuncId == numeric_limits<size_t>::max()) {
                tmpfuncId = 0;
            }
            /* if(rank == 0) { */
                /* cerr << *la << endl; */
            /* } */
            /* size_t tmplastind = numeric_limits<size_t>::max(); */
            size_t tmplastind = 0;
            MPI_ASSERT(original[i]->funcs[funcId][0]->funcname == reproduced[j]->funcs[funcId][0]->funcname);
            rq = greedyalignmentOnline(original[i]->funcs[funcId], reproduced[j]->funcs[funcId], q, la->origIndex, la->repIndex, tmpfuncId, rank, isaligned, tmplastind);
            if(lastind < tmplastind) {
                lastind = tmplastind;
            }
        }
    } else {
        firsttime = true;
    }
    /* DEBUG0("option 13\n"); */

    if(!rq.empty() && !(rq.back()->isSuccess())) {
        /* DEBUG0("at %s, we did not proceed this time 1, \ */
                /* funcId: %lu, i: %lu, j: %lu\n", \ */
                /* original[i]->bb().c_str(), funcId, i, j); */
        MPI_ASSERT(islastaligned(original, reproduced, i, j));
        rq.push_back(make_shared<lastaligned>(funcId, i, j));
        return rq;
    }

    // we expect the q to be empty through recursion
    MPI_ASSERT(q.empty());

    // in case functions were at the same element, we do the alignment the level below too
    
    /* DEBUG0("option 14\n"); */
    size_t k = -1;
    for(k = funcId + 1; k < reproduced[j]->funcs.size(); k++) {
        while(!rq.empty()) {
            MPI_ASSERT(rq.back()->isSuccess());
            rq.pop_back();
        }
        size_t tmpi = 0, tmpj = 0, tmplastind = 0;
        /* DEBUG0("k: %lu, reproduced[j]->funcs.size(): %lu\n", k, reproduced[j]->funcs.size()); */
        /* if(original[i]->funcs[k][0]->funcname != reproduced[j]->funcs[k][0]->funcname && rank == 0) { */
            /* printsurface(original[i]->funcs[k]); */
            /* DEBUG("////////////////////////////////////////////////////////////\n"); */
            /* printsurface(reproduced[j]->funcs[k]); */
            /* DEBUG("k: %lu, original[%lu]: %s, reproduced[%lu]: %s\n", k, i, original[i]->bb().c_str(), j, reproduced[j]->bb().c_str()); */
            /* DEBUG("original[%lu]->funcs[%lu][0]->funcname: %s, reproduced[%lu]->funcs[%lu][0]->funcname: %s\n", i, k, original[i]->funcs[k][0]->funcname.c_str(), j, k, reproduced[j]->funcs[k][0]->funcname.c_str()); */
        
        /* } */
        /* MPI_ASSERT(original[i]->funcs[k][0]->funcname == reproduced[j]->funcs[k][0]->funcname); */
        if(original[i]->funcs[k][0]->funcname != reproduced[j]->funcs[k][0]->funcname) {
            printf("different function called from %s: %s vs. %s\n", original[i]->bb().c_str(), original[i]->funcs[k][0]->funcname.c_str(), reproduced[j]->funcs[k][0]->funcname.c_str());
        } else {
            rq = greedyalignmentOnline(original[i]->funcs[k], reproduced[j]->funcs[k], q, tmpi, tmpj, k, rank, isaligned, tmplastind);
        }
        if(lastind < tmplastind) {
            /* DEBUG0("updating lastind: %lu at %d\n", tmplastind, __LINE__); */
            lastind = tmplastind;
        }
        if(!(((rq.empty() || rq.back()->isSuccess()) && isaligned) || \
                (k == reproduced[j]->funcs.size() - 1))) {
            /* if(rank == 0) { */
                /* printsurface(original[i]->funcs[k]); */
                /* DEBUG("////////////////////////////////////////////////////////////\n"); */
                /* printsurface(reproduced[j]->funcs[k]); */
            /* } */
        }

        MPI_ASSERT(((rq.empty() || rq.back()->isSuccess()) && isaligned) || \
                (k == reproduced[j]->funcs.size() - 1));
    }
    /* DEBUG0("option 15\n"); */

    if(!rq.empty() && !(rq.back()->isSuccess())) {
        /* DEBUG0("at %s, we did not proceed this time 2\n", \ */
        /*         original[i]->bb().c_str()); */
        MPI_ASSERT(islastaligned(original, reproduced, i, j));
        // this logic is not correct, like funcs.size() would overflow
        rq.push_back(make_shared<lastaligned>(reproduced[j]->funcs.size() - 1, i, j));
        return rq;
    }
    /* DEBUG0("option 16\n"); */

    /* DEBUG0("rq is empty\n"); */
    // let's do the greedy alignment from where we left off
    vector<pair<shared_ptr<element>, shared_ptr<element>>> aligned = vector<pair<shared_ptr<element>, shared_ptr<element>>>();

    // in case we are doing this for the first time, we should start at i and j 
    // however, if not, then you should start at the i + 1 and j + 1
    pair<size_t, size_t> p;
    size_t tmplastind = 0;
    if(firsttime) {
        /* DEBUG0("option 16.1\n"); */
        MPI_ASSERT(original[i]->funcname == reproduced[j]->funcname);
        p = __greedyalignmentOnline(original, reproduced, aligned, i, j, rank, isaligned, tmplastind);
    } else {
        /* DEBUG0("option 16.2\n"); */
        size_t tmpi = i + 1, tmpj = j + 1;
        /* DEBUG0("tmpi: %lu, tmpj: %lu, original.size(): %lu, reproduced.size(): %lu\n", tmpi, tmpj, original.size(), reproduced.size()); */
        if(tmpj == reproduced.size()) {
            p = make_pair(i, j);
        } else {
            MPI_ASSERT(tmpi < original.size());
            MPI_ASSERT(tmpj < reproduced.size());
            MPI_ASSERT(original[tmpi]->funcname == reproduced[tmpj]->funcname);
            p = __greedyalignmentOnline(original, reproduced, aligned, tmpi, tmpj, rank, isaligned, tmplastind);
        }
    }
    /* DEBUG0("option 17\n"); */
    if(lastind < tmplastind) {
        /* DEBUG0("updating lastind: %lu at %d\n", tmplastind, __LINE__); */
        lastind = tmplastind;
    }

    deque<shared_ptr<lastaligned>> qs;
    // if we found some points that are aligned, let's do the alignment level below
    bool tmpisaligned = true;
    // for each aligned pair (the 1st for loop), and for each function inside, we need to do the alignment
    size_t newfuncId;
    for(unsigned ind0 = 0; ind0 < aligned.size(); ind0++) {
        MPI_ASSERT(aligned[ind0].first->bb() == aligned[ind0].second->bb());
        MPI_ASSERT(aligned[ind0].first->funcs.size() == aligned[ind0].second->funcs.size() || ind0 == aligned.size() - 1);
        MPI_ASSERT(aligned[ind0].first->funcs.size() >= aligned[ind0].second->funcs.size());
        newfuncId = aligned[ind0].second->funcs.size() - 1;
        tmplastind = 0;
        for(unsigned ind1 = 0; ind1 < aligned[ind0].second->funcs.size(); ind1++) {
            size_t tmpi = 0, tmpj = 0, tmpfuncId = 0;
            if(aligned[ind0].first->funcs[ind1][0]->funcname != aligned[ind0].second->funcs[ind1][0]->funcname) {
                printf("different function called from %s: %s vs. %s\n", \
                        aligned[ind0].first->bb().c_str(), \
                        aligned[ind0].first->funcs[ind1][0]->funcname.c_str(), \
                        aligned[ind0].second->funcs[ind1][0]->funcname.c_str());
            } else {
                /* MPI_ASSERT(aligned[ind0].first->funcs[ind1][0]->funcname == aligned[ind0].second->funcs[ind1][0]->funcname); */
                qs = greedyalignmentOnline(aligned[ind0].first->funcs[ind1], aligned[ind0].second->funcs[ind1], q, tmpi, tmpj, tmpfuncId, rank, tmpisaligned, tmplastind); 
                // we only expect the last of the last thing to be not successful
                // why are we looking at funcids for isSuccess method?
                if((!qs.back()->isSuccess() || !tmpisaligned) && \
                        (ind0 != aligned.size() - 1 || ind1 != aligned[ind0].second->funcs.size() - 1)) {
                    DEBUG("qs.back()->isSuccess(): %d, tmpisaligned: %d\n", qs.back()->isSuccess(), tmpisaligned);
                    DEBUG("rank: %d, qs not successful, let's look\n\
                            happened at function %s\n \
                            ind0: %u, aligned.size(): %lu, ind1: %u\n \
                            aligned[%u].first->funcs.size(): %lu\n", \
                            rank, aligned[ind0].first->funcs[ind1][0]->funcname.c_str(), ind0, \
                            aligned.size(), ind1, ind0, aligned[ind0].second->funcs.size());
                    DEBUG("original\n");
                    printsurface(aligned[ind0].first->funcs[ind1]);
                    DEBUG("reproduced\n");
                    printsurface(aligned[ind0].second->funcs[ind1]);
                    cerr << qs << endl;
                }
                MPI_ASSERT((qs.back()->isSuccess() && tmpisaligned) || \
                        (ind0 == aligned.size() - 1 && \
                        ind1 == aligned[ind0].second->funcs.size() - 1));
                if(!tmpisaligned) isaligned = false;
                if(lastind < tmplastind) {
                    /* DEBUG0("updating lastind: %lu at %d\n", tmplastind, __LINE__); */
                    lastind = tmplastind;
                }
            }
        }
    }
    // if tmp said the alignment was not done at last, then we should expect the alignment did not finish at p either
    // if that is the case, push them back to the queue 

    rq = qs;
    // funcId is where you left off
    /* DEBUG0("pushing newfuncId: %lu, p.first:%lu, p.second:%lu\n", newfuncId, p.first, p.second); */
    rq.push_back(make_shared<lastaligned>(newfuncId, p.first, p.second));
    return rq;
}

// it returns true iff the traces are aligned at the end
static bool __greedyalignmentOffline(vector<shared_ptr<element>>& original, vector<shared_ptr<element>>& reproduced, vector<pair<shared_ptr<element>, shared_ptr<element>>>& aligned, const int& rank) {
    size_t i = 0, j = 0;
    unordered_map<string, vector<size_t>> bbMap;
    /* unsigned div = 0; */
    while(i < original.size() && j < reproduced.size()) {
        if(original[i]->bb() == reproduced[j]->bb()) {
            /* fprintf(stderr, "aligned: %s\n", original[i]->bb.c_str()); */
            aligned.push_back(make_pair(original[i], reproduced[j]));
            i++; j++;
        } else {
            fprintf(stderr, "option 1, rank:%d, pod:%s\n", rank, original[i - 1]->bb().c_str());
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
        fprintf(stderr, "option 2, rank:%d, pod:%s\n", rank, original[i - 1]->bb().c_str());
        /* div++; */
        return false;
    } else if(j < reproduced.size()) {
        fprintf(stderr, "option 3, rank:%d, pod:%s\n", rank, reproduced[j - 1]->bb().c_str());
        /* div++; */
        return false;
    } 

    /* DEBUG0("divergence: %u\n", div); */
    /* fprintf(stderr, "divergence: %u\n", div); */
    return true;
}

bool greedyalignmentWholeOffline(vector<shared_ptr<element>>& original, vector<shared_ptr<element>>& reproduced, const int& rank) { 
    vector<pair<shared_ptr<element>, shared_ptr<element>>> aligned;
    /* fprintf(stderr, "greedy alignment on %s at %d original.size: %lu, reproduced.size: %lu\n", \ */
            /* original[0]->bb.c_str(), rank, original.size(), reproduced.size()); */
    bool rv = __greedyalignmentOffline(original, reproduced, aligned, rank); 
    for(auto p : aligned) {
        /* cout << p.first->bb << " " << p.second->bb << endl; */
        MPI_ASSERT(p.first->bb() == p.second->bb());
        /* MPI_ASSERT(p.first->funcs.size() == p.second->funcs.size()); */
        unsigned long len = p.first->funcs.size() < p.second->funcs.size() ? p.first->funcs.size() : p.second->funcs.size();
        for(unsigned i = 0; i < len; i++) {
            // update rv at the end
            rv = greedyalignmentWholeOffline(p.first->funcs[i], p.second->funcs[i], rank);
        }
        if(p.first->funcs.size() != p.second->funcs.size()) {
            DEBUG("at %s, p.first->funcs.size(): %lu, p.second->funcs.size(): %lu\n", p.first->bb().c_str(), p.first->funcs.size(), p.second->funcs.size());
        }
    }
    return rv;
}

bool greedyalignmentWholeOffline() {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    return greedyalignmentWholeOffline(recordTraces, replayTraces, rank);
}

// returns an empty vector in case of error
vector<string> getmsgs(vector<string> &orders, const size_t lastind, unsigned& order_index) {
    size_t ind = 0;
    vector<string> msgs;
    do {
        if(order_index >= orders.size()) {
            int rank;
            MPI_Comm_rank(MPI_COMM_WORLD, &rank);
            DEBUG("order_index went beyong the size of orders, rank: %d\n", rank);
            return vector<string>();
        }
        msgs = parse(orders[order_index++], ':'); 
        ind = stoul(msgs.back());
    } while(lastind > ind);
    return msgs;
}
