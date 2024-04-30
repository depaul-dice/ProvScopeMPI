
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

lastaligned::lastaligned() : funcId(numeric_limits<unsigned long>::max()), origIndex(numeric_limits<unsigned long>::max()), repIndex(numeric_limits<unsigned long>::max()) {

}

lastaligned::lastaligned(unsigned long funcId, unsigned long origIndex, unsigned long repIndex) : funcId(funcId), origIndex(origIndex), repIndex(repIndex) {
}

bool lastaligned::isSuccess() {
    return funcId == numeric_limits<unsigned long>::max() && origIndex == numeric_limits<unsigned long>::max() && repIndex == numeric_limits<unsigned long>::max();
}

vector<shared_ptr<element>> makeHierarchyMain(vector<vector<string>>& traces, unsigned long& index) {
    DEBUG0("makeHierarchyMain called at %lu\n", index);
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
    DEBUG0("makeHierarchy for %s, at index: %lu\n", funcname.c_str(), index);
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
            printf("\t"); 
        }
        printf("%s\n", eptr->bb().c_str());

        for(unsigned i = 0; i < eptr->funcs.size(); i++) {
            print(eptr->funcs[i], depth + 1);
        }
    }
}

// this sets the pointer to the beginning of the file
static size_t findSize(FILE *fp) {
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    return size;
}

void appendReplayTrace() {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    static unsigned long index = 0;
    if(replayTraces.size() == 0) {
        DEBUG0("makeHierarchyMain called at %lu\n", index);
        MPI_ASSERT(index == 0);
        replayTraces = makeHierarchyMain(replayTracesRaw, index);
        if(rank == 0) {
            print(replayTraces, 0);
        }
    } else {
        DEBUG0("addHierarchy called at %lu\n", index);
        addHierarchy(replayTraces, replayTracesRaw, index);
        if(rank == 0) {
            print(replayTraces, 0);
        }
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


static pair<size_t, size_t> __greedyalignmentOnline(vector<shared_ptr<element>>& original, vector<shared_ptr<element>>& reproduced, vector<pair<shared_ptr<element>, shared_ptr<element>>>& aligned, size_t& initi, size_t& initj, const int& rank) {
    size_t i = initi, j = initj;
    unordered_map<string, vector<size_t>> bbMap;
    while(i < original.size() && j < reproduced.size()) {
        if(original[i]->bb() == reproduced[j]->bb()) {
            aligned.push_back(make_pair(original[i], reproduced[j]));
            i++; j++;
        } else {
            size_t oldj = j, oldi = i;
            if(bbMap.size() == 0) create_map(bbMap, reproduced, j);
            if(!matchbbmap(bbMap, original, i, j)) {
                // never aligned here
                return make_pair(oldi, oldj);
            } else {
                i++; j++;
            }
        }
    }
    if(i < original.size()) {
        return make_pair(i - 1, j - 1);
    } else {
        MPI_ASSERT(j == reproduced.size());
        return make_pair(numeric_limits<size_t>::max(), numeric_limits<size_t>::max());
    }
}

static bool issuccessful(pair<size_t, size_t> p) {
    return p.first == numeric_limits<size_t>::max() && p.second == numeric_limits<size_t>::max();
}

static bool issuccessful(queue<shared_ptr<lastaligned>>& q) {
    return q.empty();
}

queue<shared_ptr<lastaligned>> greedyalignmentOnline(vector<shared_ptr<element>>& original, vector<shared_ptr<element>>& reproduced, queue<shared_ptr<lastaligned>>& q, size_t &i, size_t &j, const int &rank) {
    // if the queue is not empty, let's do the alignment level below first
    queue<shared_ptr<lastaligned>> rq;
    if(!q.empty()) {
        shared_ptr<lastaligned> la = q.front();
        q.pop();
        rq = greedyalignmentOnline(original[la->origIndex]->funcs[la->funcId], reproduced[la->repIndex]->funcs[la->funcId], q, la->origIndex, la->repIndex, rank);
    }
    // we expect the q to be empty through recursion
    MPI_ASSERT(q.empty());

    if(!rq.empty()) {
        // I need to make sure that the last alignment is the same as before
        rq.push(make_shared<lastaligned>(i, j, rq.front()->funcId));
        return rq;
    }

    // let's do the greedy alignment from where we left off
    vector<pair<shared_ptr<element>, shared_ptr<element>>> aligned = vector<pair<shared_ptr<element>, shared_ptr<element>>>();
    pair<size_t, size_t> p = __greedyalignmentOnline(original, reproduced, aligned, i, j, rank);
    queue<shared_ptr<lastaligned>> qs;
    // if we found some points that are aligned, let's do the alignment level below
    for(unsigned ind0 = 0; ind0 < aligned.size(); ind0++) {
        MPI_ASSERT(aligned[ind0].first->bb() == aligned[ind0].second->bb());
        MPI_ASSERT(aligned[ind0].first->funcs.size() == aligned[ind0].second->funcs.size());
        for(unsigned ind1 = 0; ind1 < aligned[ind0].first->funcs.size(); ind1++) {
            size_t tmpi = 0, tmpj = 0;
            qs = greedyalignmentOnline(aligned[ind0].first->funcs[ind1], aligned[ind0].second->funcs[ind1], q, tmpi, tmpj, rank); 
            // we only expect the last of the last thing to be not successful
            MPI_ASSERT(issuccessful(qs) || (ind0 == aligned.size() - 1 && \
                    ind1 == aligned[ind0].first->funcs.size() - 1));
        }
    }
    // if tmp said the alignment was not done at last, then we should expect the alignment did not finish at p either
    // if that is the case, push them back to the queue 
    if(!issuccessful(qs)) {
        MPI_ASSERT(!issuccessful(p));
        rq = qs;
        rq.push(make_shared<lastaligned>(p.first, p.second, aligned.back().first->funcs.size()));
    }
    return rq;
}
