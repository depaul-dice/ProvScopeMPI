
#include "tools.h"

using namespace std;

/* set<string> testfamily {"MPI_Test", "MPI_Testall", "MPI_Testany", "MPI_Testsome"}; */
/* set<string> waitfamily {"MPI_Wait", "MPI_Waitall", "MPI_Waitany", "MPI_Waitsome"}; */

#ifdef DEBUG_MODE

static FILE* debugfile = nullptr;

void open_debugfile() {
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    char filename[100];
    sprintf(filename, "debugfile_%d.txt", rank);
    debugfile = fopen(filename, "w");
    if(debugfile == nullptr) {
        fprintf(stderr, "failed to open debugfile\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    fprintf(stderr, "%s opened\n", filename);
}

void close_debugfile() {
    if(debugfile != nullptr) {
        fclose(debugfile);
    }
}

/* void DEBUG(const char* format, ...) { */
/*     vfprintf(stderr, format, args); */
/*     va_end(args); */
/*     va_list args; */
/*     va_start(args, format); */
/*     if(debugfile == nullptr) { */
/*         MPI_ASSERT(false); */
/*     } */
/*     vfprintf(debugfile, format, args); */
/*     va_end(args); */
/*     fflush(debugfile); */
/* } */

#endif // DEBUG_MODE

vector<string> parse(string line, char delimit) {
    vector<string> res;
    string tmp = "";
    for (unsigned i = 0; i < line.size(); i++) {
        if (line[i] == delimit) {
            res.push_back(tmp);
            tmp = "";
        } else {
            tmp += line[i];
        }
    }
    res.push_back(tmp);
    return res;
}

int lookahead(vector<string>& orders, unsigned start, string& request) {
    vector<string> tokens;
    for (unsigned i = start; i < orders.size(); i++) {
        tokens = parse(orders[i], ':');
        if(tokens[0] == "MPI_Test") {
            // either MPI_Test:rank:request:SUCCESS:src
            // or MPI_Test:rank:request:FAIL
            if(tokens[2] == request) {
                if(tokens[3] == "SUCCESS") {
                    return stoi(tokens[4]);
                } 
            }
        } else if (tokens[0] == "MPI_Testall") {
            // have not implemented this yet
            MPI_ASSERT(false);
        } else if (tokens[0] == "MPI_Testany") {
            // have not implemented this yet 
            MPI_ASSERT(false);
        } else if (tokens[0] == "MPI_Testsome") {
            // either MPI_Testsome:rank:outcount:request:src:... 
            // or MPI_Testsome:rank:outcount
            int outcount = stoi(tokens[2]);
            if(outcount == 0) continue;
            else {
                for(int j = 0; j < outcount; j++) {
                    if(tokens[3 + 2 * j] == request) {
                        return stoi(tokens[3 + 2 * j + 1]);
                    }
                }
            }
        } else if (tokens[0] == "MPI_Wait") {
            // MPI_Wait:rank:request:SUCCESS:src
            // or MPI_Wait:rank:request:FAIL
            if(tokens[2] == request && tokens[3] == "SUCCESS") {
                return stoi(tokens[4]);
            }
        } else if (tokens[0] == "MPI_Waitall") {
            // have not implemented this yet
            MPI_ASSERT(false);
        } else if (tokens[0] == "MPI_Waitany") {
            // MPI_Waitany:rank:SUCCESS:request:src
            // or MPI_Waitany:rank:FAIL
            if(tokens[2] == "SUCCESS" && tokens[3] == request) {
                return stoi(tokens[4]);
            }
        } else if (tokens[0] == "MPI_Waitsome") {
            // have not implemented this yet
            MPI_ASSERT(false);
        } else if (tokens[0] == "MPI_Cancel") {
            // MPI_Cancel:rank:request
            if(tokens[2] == request) {
                return -2;
            }
        } else {
            // do nothing
        } 

        // if not, just continue
    }
    return -1;
}

void printtails(vector<vector<string>>& traces, unsigned tail) {
    /* MPI_ASSERT(tail <= traces.size()); */
    if(tail > traces.size()) {
        fprintf(stderr, "tail is larger than traces.size()\n");
        return;
    }
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    for(unsigned i = traces.size() - tail; i < traces.size(); i++) {
        /* MPI_ASSERT(traces[i].size() == 3); */
        if(traces[i].size() != 3) {
            fprintf(stderr, "%d:%s\n", rank, traces[i][0].c_str());
            return;
        }
        fprintf(stderr, "%d:%s:%s:%s\n", rank, traces[i][0].c_str(), traces[i][1].c_str(), traces[i][2].c_str());
    }
}


Logger::Logger(): rank(-1), initialized(0) {
    MPI_Initialized(&initialized);
    if(initialized) {
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    }
    /* MPI_Comm_rank(MPI_COMM_WORLD, &rank); */
}

Logger& Logger::operator<<(ostream& (*manip)(ostream&)) {
    if(rank == -1 && !initialized) {
        MPI_Initialized(&initialized);
        if(initialized) {
            MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        } else {
            cerr << "called logger too early" << endl;
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }
    if(rank == 0) {
        cerr << manip;
    }
    return *this;
}

Logger::~Logger() {
}

string replaceall(string& str, const string& from, const string& to) {
    string ret = str;
    size_t start_pos = 0;
    while((start_pos = ret.find(from, start_pos)) != string::npos) {
        ret.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    return ret;
}

void splitNinsert(const string& str, const string& delimit, unordered_set<string>& container) {
    size_t start = 0;
    size_t end = str.find(delimit);
    string res;
    while(end != string::npos) {
        res = str.substr(start, end - start);
        if(res.size() > 0) {
            container.insert(res);
        }
        start = end + delimit.size();
        end = str.find(delimit, start);
    }
    res = str.substr(start, end);
    if(res.size() > 0) {
        container.insert(res);
    }
}
