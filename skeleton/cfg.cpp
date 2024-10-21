
#include "cfg.h"

using namespace std;

node::node(const string& name) : 
    name(name) {}

cfg::cfg(const string& funcname): 
    funcname(funcname), entry(nullptr) {}

cfg::~cfg() {
    for (auto it = nodes.begin(); it != nodes.end(); it++) {
        delete it->second;
    }
}

void cfg::insertNode(const string &name) {
    if(nodes.find(name) == nodes.end()) {
        nodes[name] = new node(name);
    } 
}

void cfg::insertEdge(
        const string &from, const string &to) {
    node *src = nodes[from], *dst = nodes[to];
    if(outEdges.find(src) == outEdges.end()) {
        outEdges[src] = set<node *>();
    }
    outEdges[src].insert(dst);
    if(inEdges.find(dst) == inEdges.end()) {
        inEdges[dst] = set<node *>();
    }
    inEdges[dst].insert(src);
}

set<node *> cfg::getNodes() {
    set<node *> ret{};
    for (auto it = nodes.begin(); it != nodes.end(); it++) {
        ret.insert(it->second);
    }
    return ret;
}

node *cfg::getEntry() {
    if(entry) return entry;
    else {
        for(auto it = nodes.begin(); it != nodes.end(); it++) {
            if(inEdges.find(it->second) == inEdges.end()) {
                entry = it->second;
                return entry;
            }
        }
        return nullptr;
    }
}

node *cfg::getNode(const string &name) {
    auto it = nodes.find(name);
    if(it != nodes.end()) {
        return it->second;
    } else {
        return nullptr;
    }
}

static void mergeEdges(
        map<node *, set<node *>> &tmpOutEdges, 
        map<node *, set<node *>> &outEdges, 
        map<node *, set<node *>> &inEdges) {
    for(auto it = tmpOutEdges.begin(); it != tmpOutEdges.end(); it++) {
        if(outEdges.find(it->first) == outEdges.end()) {
            outEdges[it->first] = set<node *>();
        }
        for(auto it2 = it->second.begin(); it2 != it->second.end(); it2++) {
            outEdges[it->first].insert(*it2);
            if(inEdges.find(*it2) == inEdges.end()) {
                inEdges[*it2] = set<node *>();
            }
            inEdges[*it2].insert(it->first);
        }
    }
    tmpOutEdges.clear();
}

set<node *> cfg::findMinimalNodes() {
    set<node *> ret{};
    vector<node *> v{};    
    for (auto it = nodes.begin(); it != nodes.end(); it++) {
        v.push_back(it->second);
    }
    srand(unsigned(time(nullptr)));
    random_shuffle(v.begin(), v.end());

    map<node *, set<node *>> newOutEdges = outEdges, newInEdges = inEdges, tmpOutEdges;
    bool flag = false;
    for(node *n : v) {
        flag = false;
        if(newInEdges.find(n) == newInEdges.end() 
                || newInEdges[n].size() == 0
                || newOutEdges.find(n) == newOutEdges.end() 
                || newOutEdges[n].size() == 0) {
            continue;
        }
        for(node *src : newInEdges[n]) {
            for(node *dst : newOutEdges[n]) {
                if(newOutEdges.find(src) != newOutEdges.end() 
                        && newOutEdges[src].find(dst) != newOutEdges[src].end()) {
                    flag = true;
                    break;
                } else {
                    if(tmpOutEdges.find(src) == tmpOutEdges.end()) {
                        tmpOutEdges[src] = set<node *>();
                    }
                    tmpOutEdges[src].insert(dst);
                }
            }
            if(flag) break;
        }
        if(!flag) {
            llvm::errs() << "Removing node: " << n->name << "\n";
            ret.insert(n);
            mergeEdges(tmpOutEdges, newOutEdges, newInEdges);
        }
    }
    return ret;
}
