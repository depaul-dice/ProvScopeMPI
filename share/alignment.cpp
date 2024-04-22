
#include "alignment.h"

using namespace std;

/* static FILE* replayTraceFile = nullptr; */
// recordTracesRaw is simply a local variable, so don't make it a global variable
vector<shared_ptr<element>> recordTraces;

vector<vector<string>> replayTracesRaw; // this is necessary for bbprinter
vector<shared_ptr<element>> replayTraces;
unordered_map<string, vector<unsigned>> bbMap;

element::element(bool isEntry, bool isExit, string& bb) : isEntry(isEntry), isExit(isExit), bb(bb), funcs(vector<vector<shared_ptr<element>>>()) {
}

vector<shared_ptr<element>> makeHierarchyWhole(vector<vector<string>>& traces, unsigned long& index) {
    vector<shared_ptr<element>> functionalTraces;
    bool isEntry, isExit;
    string bbname, funcname;
    /* DEBUG0("index: %lu\n", index); */
    funcname = traces[index][0];
    /* DEBUG0("funcname: %s\n", funcname.c_str()); */
    do {
        /* funcname = traces[index][0]; */
        bbname = traces[index][0] + ":" + traces[index][1] + ":" + traces[index][2];
        isEntry = (traces[index][1] == "entry" || traces[index][1] == "both");
        isExit = (traces[index][1] == "exit" || traces[index][1] == "both");
        /* element e(isEntry, isExit, bbname); */
        shared_ptr<element> eptr = make_shared<element>(isEntry, isExit, bbname);
        index++;
        while(!isExit && funcname != traces[index][0]) {
            MPI_ASSERT(traces[index][1] == "entry" || traces[index][1] == "both");
            eptr->funcs.push_back(makeHierarchyWhole(traces, index)); 
        }
        functionalTraces.push_back(eptr);
    } while(!isExit);

    return functionalTraces; 
}

void print(vector<shared_ptr<element>>& functionalTraces, unsigned depth) {
    for(auto& eptr : functionalTraces) {
        for(unsigned i = 0; i < depth; i++) {
            cout << "\t";
        }
        cout << eptr->bb << endl;

        for(unsigned i = 0; i < eptr->funcs.size(); i++) {
            print(eptr->funcs[i], depth + 1);
        }
    }
}

static size_t findSize(FILE *fp) {
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    return size;
}

void appendReplayTrace() {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    /* if(replayTraceFile == nullptr) { */
    /*     char filename[100]; */
    /*     sprintf(filename, ".record%d.tr", rank); */
    /*     replayTraceFile = fopen(filename, "r"); */
    /*     size_t filesize = findSize(replayTraceFile); */
    /*     MPI_ASSERT(replayTraceFile != nullptr); */
    /*     DEBUG("opened file %s\n", filename); */
    /*     MPI_ASSERT(filesize > 0); */
    /* } */

    /* char* line = nullptr; */ 
    /* size_t len = 0; */
    /* ssize_t read; */
    /* while((read = getline(&line, &len, replayTraceFile)) != -1) { */
    /*     if(read > 0 && line[read - 1] == '\n') { */
    /*         line[read - 1] = '\0'; */
    /*     } */
    /*     string str(line); */
    /*     replayTracesRaw.push_back(parse(str, ':')); */
    /* } */
    /* MPI_ASSERT(replayTracesRaw.size() > 0); */
    unsigned long index = 0;
    // this is as of now 
    /* for(unsigned i = 0; i < replayTracesRaw.size(); i++) { */
    /*     fprintf(stderr, "%s:%s:%s\n", replayTracesRaw[i][0].c_str(), replayTracesRaw[i][1].c_str(), replayTracesRaw[i][2].c_str()); */
    /* } */
    replayTraces = makeHierarchyWhole(replayTracesRaw, index);
    
    /* print(replayTraces, 0); */
    return;
}

static inline void create_map(vector<shared_ptr<element>>& reproduced, unsigned& j) {
    for(unsigned k = j; k < reproduced.size(); k++) {
        if(bbMap.find(reproduced[k]->bb) == bbMap.end()) {
            bbMap[reproduced[k]->bb] = vector<unsigned>();
        }
        bbMap[reproduced[k]->bb].push_back(k);
    }
    return;
}

static inline bool matchbbmap(vector<shared_ptr<element>>& original, unsigned& i, unsigned& j) {
    for(unsigned k = i; k < original.size(); k++) {
        if(bbMap.find(original[k]->bb) != bbMap.end()) {
            for(auto l: bbMap[original[k]->bb]) {
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

static bool __greedyalignment(vector<shared_ptr<element>>& original, vector<shared_ptr<element>>& reproduced, vector<pair<shared_ptr<element>, shared_ptr<element>>>& aligned) {
    unsigned i = 0, j = 0;
    while(i < original.size() && j < reproduced.size()) {
        if(original[i]->bb == reproduced[j]->bb) {
            aligned.push_back(make_pair(original[i], reproduced[j]));
            i++; j++;
        } else {
            fprintf(stderr, "point of divergence: %s", original[i - 1]->bb.c_str());
            if(bbMap.size() == 0) create_map(reproduced, j);

            // matchbbmap will update i and j
            if(!matchbbmap(original, i, j)) return false;
            else {
                fprintf(stderr, "point of convergence: %s", original[i]->bb.c_str());
                aligned.push_back(make_pair(original[i], reproduced[j]));
                i++; j++;
            }
        }
    }
    return true;
}

bool greedyalignmentWhole(vector<shared_ptr<element>>& original, vector<shared_ptr<element>>& reproduced) { 
    vector<pair<shared_ptr<element>, shared_ptr<element>>> aligned;
#ifdef DEBUG_MODE
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    fprintf(stderr, "greedy alignment on %s at %d\n", original[0]->bb.c_str(), rank);
#endif
    bool rv = __greedyalignment(original, reproduced, aligned); 
    for(auto p : aligned) {
        /* cout << p.first->bb << " " << p.second->bb << endl; */
        MPI_ASSERT(p.first->bb == p.second->bb);
        MPI_ASSERT(p.first->funcs.size() == p.second->funcs.size());
        for(unsigned i = 0; i < p.first->funcs.size(); i++) {
            // update rv at the end
            rv = greedyalignmentWhole(p.first->funcs[i], p.second->funcs[i]);
        }
    }
    return rv;
}

bool greedyalignmentWhole() {
    return greedyalignmentWhole(recordTraces, replayTraces);
}

