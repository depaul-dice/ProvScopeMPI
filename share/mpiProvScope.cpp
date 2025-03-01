
#include "mpiRecordReplay.h"
/* #define PROVRECORD */

using namespace std;

Logger logger;

#ifndef PROVRECORD

#include <chrono> 

extern vector<shared_ptr<element>> recordTraces;
extern vector<vector<string>> replayTracesRaw;

/*
 * This is referring to points of divergences (also named pods,
 * of course) in alignment.cpp.
 * It stores points of divergences at dump it at MPI_Finalize.
 */
extern unordered_set<string> pods;

/* 
 * key is the function name
 */
static unordered_map<string, loopNode *> __loopTrees;

/* 
 * The name passed down as an argument will be in the format 
 * FunctionName:Type:Count:(node count)
 * Type is a type of a node (e.g. entry node, exit node, neither)
 * Count is a unique identifier
 * node count is the counter for nodes to be passed
 */
extern "C" void printBBname(const char *name) {
    int rank, flag1, flag2;
    MPI_Initialized(&flag1);
    MPI_Finalized(&flag2);
    vector<string> tokens;
    if(flag1 && !flag2) {
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        string str(name);
        tokens = parse(str, ':');
        replayTracesRaw.push_back(tokens);
        // if it is a replay trace the trace length will be 3, but for record 4
        /* MPI_ASSERT(replayTracesRaw.size() > 0); */
    }
}

int MPI_Init(
    int *argc, 
    char ***argv
) {
    int ret = PMPI_Init(argc, argv);
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    /*
     * opening the file that records the traces
     */
    char *line = nullptr;
    size_t len = 0;
    ssize_t read;

    string tracefile = ".record" + to_string(rank) + ".tr";
    vector<vector<string>> rawTraces;
    FILE *fp = fopen(tracefile.c_str(), "r");
    if(fp == nullptr) {
        fprintf(stderr, "Error: cannot open file %s\n", tracefile.c_str());
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    while((read = getline(&line, &len, fp)) != -1) {
        if(read > 0 && line[read - 1] == '\n') {
            line[read - 1] = '\0';
        }
        string str(line);
        rawTraces.push_back(parse(str, ':'));
        if(line != nullptr) {
            free(line);
            line = nullptr;
        }
    }
    if(line != nullptr) {
        free(line);
        line = nullptr;
    }
    fclose(fp);

    MPI_ASSERT(rawTraces.size() > 0);

    /*
     * opening the file that has loop information
     */
    string loopTreeFile = "loops.dot";
    __loopTrees = parseDotFile(loopTreeFile);
    for(auto lt: __loopTrees) {
        lt.second->fixExclusives();
    }

    /*
     * creating the hierarchy of traces for recorded traces
     */
    unsigned long index = 0;
    auto tik = chrono::high_resolution_clock::now();
    recordTraces = makeHierarchyMain(
            rawTraces, 
            index, 
            __loopTrees); 
    auto tok = chrono::high_resolution_clock::now();
    if(rank == 0) {
        printf("making Hierarchy took %ld ms\n", 
                chrono::duration_cast<chrono::milliseconds>(tok - tik).count());
    }

    return ret;
}

int MPI_Finalize() {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    auto tik = chrono::high_resolution_clock::now();
    /* DEBUG("MPI_Finalize:%d\n", rank); */
    appendTraces(__loopTrees, TraceType::REPLAY);
    auto tok = chrono::high_resolution_clock::now();
    auto unrollTime = chrono::duration_cast<chrono::microseconds>(tok - tik).count();
    greedyAlignmentWholeOffline(); 

    /*
     * deleting the looptrees
     */
    for(auto it = __loopTrees.begin(); it != __loopTrees.end(); it++) {
        delete it->second;
    }

    /*
     * print the pods to a file 
     * (to avoid writing everything at console at once)
     */
    string podfile = "pods" + to_string(rank) + ".txt";
    FILE *fp = fopen(podfile.c_str(), "w");
    for(auto it = pods.begin(); it != pods.end(); it++) {
        fprintf(fp, "%s\n", it->c_str());
    }
    fclose(fp);

    printf("Unroll time:%ld microseconds\n", unrollTime);

    int rv = PMPI_Finalize();
    return rv;
}

#else
FILE *traceFile = nullptr;
unsigned long nodecnt = 0;

extern "C" void printBBname(const char *name) {
    int rank, flag1, flag2;
    MPI_Initialized(&flag1);
    MPI_Finalized(&flag2);
    if(flag1 && !flag2) {
        char filename[100];
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        MPI_ASSERT(traceFile != nullptr);
        fprintf(traceFile, 
                "%s:%lu\n", 
                name, 
                nodecnt++);
    }
    return;
}

int MPI_Init(
    int *argc, 
    char ***argv
) {
    int ret = PMPI_Init(argc, argv);
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    //DEBUG("MPI_Init rank:%d\n", rank);

    /*
     * open the trace file
     */
    string filename = ".record" + to_string(rank) + ".tr";
    traceFile = fopen(filename.c_str(), "w");
    if(traceFile == nullptr) {
        fprintf(stderr, "failed to open trace file\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    return ret;
}

int MPI_Finalize(void) {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_ASSERT(traceFile != nullptr);
    fflush(traceFile);
    fclose(traceFile);
    int ret = PMPI_Finalize();
    /* DEBUG("MPI_Finalize done, rank:%d\n", rank); */
    return ret;
}

#endif // PROVRECORD
