
#include "tools.h"

using namespace std;

/* set<string> testfamily {"MPI_Test", "MPI_Testall", "MPI_Testany", "MPI_Testsome"}; */
/* set<string> waitfamily {"MPI_Wait", "MPI_Waitall", "MPI_Waitany", "MPI_Waitsome"}; */

vector<string> parse(string& line, char delimit) {
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
        } 
        // if not, just continue
    }
    return -1;
}
