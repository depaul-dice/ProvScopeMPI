
#include "parse.h"

using namespace std;

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
