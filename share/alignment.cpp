
#include "alignment.h"

using namespace std;

/* static FILE* replayTraceFile = nullptr; */
vector<vector<string>> replayTracesRaw;
vector<shared_ptr<element>> replayTraces;

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
            MPI_ASSERT(traces[index][1] == "entry");
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

void alignment(vector<element>& original, vector<element>& reproduced) {
    

}
