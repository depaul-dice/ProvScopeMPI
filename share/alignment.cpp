
#include "alignment.h"

using namespace std;

/* static FILE* replayTraceFile = nullptr; */
// recordTracesRaw is simply a local variable, so don't make it a global variable
vector<shared_ptr<element>> recordTraces;

vector<vector<string>> replayTracesRaw; // this is necessary for bbprinter
vector<shared_ptr<element>> replayTraces;

/* element::element(bool isEntry, bool isExit, string& bb) : isEntry(isEntry), isExit(isExit), bb(bb), funcs(vector<vector<shared_ptr<element>>>()) { */
/* } */

element::element(bool isEntry, bool isExit, int id, string& funcname) : funcs(vector<vector<shared_ptr<element>>>()), funcname(funcname), id(id), isEntry(isEntry), isExit(isExit) {
}

string element::bb() const {
    if(isEntry && isExit) {
        return funcname + ":both:" + to_string(id);
    } else if(isEntry) {
        return funcname + ":entry:" + to_string(id);
    } else if(isExit) {
        return funcname + ":exit:" + to_string(id);
    } else {
        return funcname + ":neither:" + to_string(id);
    }
}

bool element::operator ==(const element &e) const {
    return isEntry == e.isEntry && isExit == e.isExit && id == e.id && funcname == e.funcname;
}

lastaligned::lastaligned() : funcId(numeric_limits<unsigned long>::max()), origIndex(numeric_limits<unsigned long>::max()), repIndex(numeric_limits<unsigned long>::max()) {
}

lastaligned::lastaligned(unsigned long funcId, unsigned long origIndex, unsigned long repIndex) : funcId(funcId), origIndex(origIndex), repIndex(repIndex) {
}

bool lastaligned::isSuccess() {
    return funcId == numeric_limits<unsigned long>::max() && origIndex == numeric_limits<unsigned long>::max() && repIndex == numeric_limits<unsigned long>::max();
}

ostream& operator<<(ostream& os, const lastaligned& l) {
    os << "funcId: " << l.funcId << ", origIndex: " << l.origIndex << ", repIndex: " << l.repIndex;
    return os;
}

vector<shared_ptr<element>> makeHierarchyMain(vector<vector<string>>& traces, unsigned long& index) {
    /* DEBUG0("makeHierarchyMain called at %lu\n", index); */
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
        MPI_ASSERT(traces[index].size() == 3);
        bbname = traces[index][0] + ":" + traces[index][1] + ":" + traces[index][2]; 
        isEntry = (traces[index][1] == "entry");
        isExit = (traces[index][1] == "exit");
        /* DEBUG0("bbname: %s\n", bbname.c_str()); */
        shared_ptr<element> eptr = make_shared<element>(isEntry, isExit, stoi(traces[index][2]), traces[index][0]);
        index++;
        
        while(!isExit && index < traces.size() && traces[index][1] == "entry") {
            eptr->funcs.push_back(makeHierarchy(traces, index));
        }
        functionalTraces.push_back(eptr);
    } while(!isExit && index < traces.size());

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
    /* DEBUG0("makeHierarchy for %s, at index: %lu\n", funcname.c_str(), index); */
    /* DEBUG0("funcname: %s: %lu\n", funcname.c_str(), index); */
    do {
        MPI_ASSERT(index < traces.size());
        bbname = traces[index][0] + ":" + traces[index][1] + ":" + traces[index][2];
        if(traces[index][0] != funcname) {
            DEBUG0("bbname: %s\n", bbname.c_str());
            DEBUG0("funcname: %s, traces[index][0]: %s\n", funcname.c_str(), traces[index][0].c_str());
        }
        MPI_ASSERT(traces[index][0] == funcname);
        MPI_ASSERT(traces[index].size() == 3);
        /* DEBUG0("bbname: %s\n", bbname.c_str()); */
        isEntry = (traces[index][1] == "entry");
        isExit = (traces[index][1] == "exit");
        /* shared_ptr<element> eptr = make_shared<element>(isEntry, isExit, bbname); */
        shared_ptr<element> eptr = make_shared<element>(isEntry, isExit, stoi(traces[index][2]), traces[index][0]);
        index++;
        while(!isExit && index < traces.size() && traces[index][1] == "entry") {
            eptr->funcs.push_back(makeHierarchy(traces, index)); 
        }
        functionalTraces.push_back(eptr);
    } while(!isExit && index < traces.size());

    return functionalTraces; 
}

void addHierarchy(vector<shared_ptr<element>>& functionalTraces, vector<vector<string>>& traces, unsigned long& index) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    stack<shared_ptr<element>> __stack;
    MPI_ASSERT(!functionalTraces.empty());
    MPI_ASSERT(functionalTraces[0]->funcname == "main");
    MPI_ASSERT(functionalTraces.back()->funcname == "main");
    shared_ptr<element> curr = functionalTraces.back();


    while(!(curr->isExit)) {
        /* DEBUG0("lastElement->bb(): %s\n", lastElement->bb().c_str()); */
        __stack.push(curr);
        if(curr->funcs.size() == 0) {
            break;
        } else {
            curr = curr->funcs.back().back();
        }
    }
    /* DEBUG0("stack ready\n"); */
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

    while(index < traces.size()) {
        while (index < traces.size() && traces[index][1] == "entry") {
            curr->funcs.push_back(makeHierarchy(traces, index));
        } 
        if(index >= traces.size()) {
            break;
        } 
        MPI_ASSERT(traces[index][1] != "entry");
        /* DEBUG0("%s:%s:%s\n", traces[index][0].c_str(), traces[index][1].c_str(), traces[index][2].c_str()); */
        newchild = make_shared<element>(traces[index][1] == "entry", traces[index][1] == "exit", stoi(traces[index][2]), traces[index][0]);
        MPI_ASSERT(traces[index][0] == curr->funcname);
        index++;
        if(parent != nullptr) {
            parent->funcs.back().push_back(newchild);
        } else {
            functionalTraces.push_back(newchild);
        }
        if(traces[index - 1][1] == "exit") { // this index - 1 is the last index
            if(parent == nullptr) {
                break; // this should be at the end of main
            } else {
                curr = parent;
                if(!__stack.empty()) {
                    parent = __stack.top();
                    __stack.pop();
                } else {
                    parent = nullptr;
                }
            }
        } else {
            curr = newchild;
        }
    }
}

void print(vector<shared_ptr<element>>& functionalTraces, unsigned depth) {
    for(auto& eptr : functionalTraces) {
        for(unsigned i = 0; i < depth; i++) {
            fprintf(stderr, "\t"); 
        }
        fprintf(stderr, "%s\n", eptr->bb().c_str());

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

deque<shared_ptr<lastaligned>> onlineAlignment(deque<shared_ptr<lastaligned>>& q, bool& isaligned) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    /* static deque<shared_ptr<lastaligned>> q; */
    appendReplayTrace();
    size_t i = 0, j = 0; // this part is not correct, you need to read it from q
    size_t funcId = 0;
    if(!q.empty()) {
        i = q.back()->origIndex;
        j = q.back()->repIndex;
        funcId = q.back()->funcId;
        q.pop_back();
    }
    /* DEBUG0("online alignment starting\n"); */
    deque<shared_ptr<lastaligned>> rq;
    rq = greedyalignmentOnline(recordTraces, replayTraces, q, i, j, funcId, rank, isaligned);
    /* if(rank == 0) { */
        /* cerr << "printing rq at the end\n" << rq << endl; */
    /* } */
    /* DEBUG0("online alignment done\n"); */
    return rq;
}

void appendReplayTrace() {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    static unsigned long index = 0;
    if(replayTraces.size() == 0) {
        /* DEBUG0("makeHierarchyMain called at %lu\n", index); */
        MPI_ASSERT(index == 0);
        replayTraces = makeHierarchyMain(replayTracesRaw, index);
        /* if(rank == 0) { */
            /* print(replayTraces, 0); */
        /* } */
    } else {
        /* DEBUG0("addHierarchy called at %lu\n", index); */
        addHierarchy(replayTraces, replayTracesRaw, index);
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


static pair<size_t, size_t> __greedyalignmentOnline(vector<shared_ptr<element>>& original, vector<shared_ptr<element>>& reproduced, vector<pair<shared_ptr<element>, shared_ptr<element>>>& aligned, size_t& initi, size_t& initj, const int& rank, bool&isaligned) {
    /* DEBUG0("__greedyalignmentOnline at %s, i: %lu, j: %lu, ", \ */
    /*         original[initi]->bb().c_str(), initi, initj); */
    size_t i = initi, j = initj;
    unordered_map<string, vector<size_t>> bbMap;
    while(i < original.size() && j < reproduced.size()) {
        if(original[i]->bb() == reproduced[j]->bb()) {
            aligned.push_back(make_pair(original[i], reproduced[j]));
            i++; j++;
        } else {
            if(i != initi || j != initj) {
                printf("pod: %s, rank: %d\n", original[i - 1]->bb().c_str(), rank);
            }
            size_t oldj = j, oldi = i;
            if(bbMap.size() == 0) create_map(bbMap, reproduced, j);
            if(!matchbbmap(bbMap, original, i, j)) {
                // never aligned here
                DEBUG0("never aligned at i: %lu, j: %lu, from %s\n", oldi, oldj, original[oldi]->bb().c_str());
                /* if(rank == 0) { */
                    /* DEBUG0("original\n"); */
                    /* printsurface(original); */
                    /* DEBUG0("reproduced\n"); */
                    /* printsurface(reproduced); */
                /* } */
                isaligned = false;
                return make_pair(oldi, oldj);
            } else {
                printf("poc: %s, rank: %d\n", original[i]->bb().c_str(), rank);
                i++; j++;
            }
        }
    }
    isaligned = true;
    if(i < original.size()) {
        /* DEBUG0("did not finish at i: %lu\n", i - 1); */
        return make_pair(i - 1, j - 1);
    } else {
        MPI_ASSERT(j == reproduced.size());
        /* DEBUG0("aligned till the end\n"); */
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

deque<shared_ptr<lastaligned>> greedyalignmentOnline(vector<shared_ptr<element>>& original, vector<shared_ptr<element>>& reproduced, deque<shared_ptr<lastaligned>>& q, size_t &i, size_t &j, const size_t& funcId, const int &rank, bool& isaligned) {
    // if the queue is not empty, let's do the alignment level below first
    deque<shared_ptr<lastaligned>> rq;
    /* DEBUG0("greedyalignmentOnline at %s with i: %lu, j: %lu, funcId:%lu\n",\ */
            /* original[i]->bb().c_str(), i, j, funcId); */

    // in case the q is not empty, we need to do the alignment level below first
    bool firsttime = false;
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
            rq = greedyalignmentOnline(original[i]->funcs[funcId], reproduced[j]->funcs[funcId], q, la->origIndex, la->repIndex, tmpfuncId, rank, isaligned);
        }
    } else {
        firsttime = true;
    }

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
    
    size_t k = -1;
    for(k = funcId + 1; k < reproduced[j]->funcs.size(); k++) {
        while(!rq.empty()) {
            MPI_ASSERT(rq.back()->isSuccess());
            rq.pop_back();
        }
        size_t tmpi = 0, tmpj = 0;
        /* DEBUG0("k: %lu, reproduced[j]->funcs.size(): %lu\n", k, reproduced[j]->funcs.size()); */
        rq = greedyalignmentOnline(original[i]->funcs[k], reproduced[j]->funcs[k], q, tmpi, tmpj, k, rank, isaligned);
        MPI_ASSERT(((rq.empty() || rq.back()->isSuccess()) && isaligned) || \
                (k == reproduced[j]->funcs.size() - 1));
    }

    if(!rq.empty() && !(rq.back()->isSuccess())) {
        /* DEBUG0("at %s, we did not proceed this time 2\n", \ */
        /*         original[i]->bb().c_str()); */
        MPI_ASSERT(islastaligned(original, reproduced, i, j));
        // this logic is not correct, like funcs.size() would overflow
        rq.push_back(make_shared<lastaligned>(reproduced[j]->funcs.size() - 1, i, j));
        return rq;
    }

    /* DEBUG0("rq is empty\n"); */
    // let's do the greedy alignment from where we left off
    vector<pair<shared_ptr<element>, shared_ptr<element>>> aligned = vector<pair<shared_ptr<element>, shared_ptr<element>>>();

    // in case we are doing this for the first time, we should start at i and j 
    // however, if not, then you should start at the i + 1 and j + 1
    pair<size_t, size_t> p;
    if(firsttime) {
        p = __greedyalignmentOnline(original, reproduced, aligned, i, j, rank, isaligned);
    } else {
        size_t tmpi = i + 1, tmpj = j + 1;
        p = __greedyalignmentOnline(original, reproduced, aligned, tmpi, tmpj, rank, isaligned);
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
        for(unsigned ind1 = 0; ind1 < aligned[ind0].second->funcs.size(); ind1++) {
            size_t tmpi = 0, tmpj = 0, tmpfuncId = 0;
            qs = greedyalignmentOnline(aligned[ind0].first->funcs[ind1], aligned[ind0].second->funcs[ind1], q, tmpi, tmpj, tmpfuncId, rank, tmpisaligned); 
            // we only expect the last of the last thing to be not successful
            /* if((!qs.back()->isSuccess() || !tmpisaligned) && \ */
            /*         (ind0 != aligned.size() - 1 || ind1 != aligned[ind0].second->funcs.size() - 1)) { */
            /*     DEBUG0("rank: %d, qs not successful, let's look\n\ */
            /*             ind0: %u, aligned.size(): %lu, ind1: %u, \ */
            /*             aligned[%u].first->funcs.size(): %lu", \ */
            /*             rank, ind0, aligned.size(), ind1, ind0, aligned[ind0].second->funcs.size()); */
            /*     if(rank == 0) { */
            /*         cerr << qs << endl; */
            /*     } */
            /* } */
            MPI_ASSERT((qs.back()->isSuccess() && tmpisaligned) || \
                    (ind0 == aligned.size() - 1 && \
                    ind1 == aligned[ind0].second->funcs.size() - 1));
            if(!tmpisaligned) isaligned = false;
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


