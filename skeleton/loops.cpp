
#include "loops.h"

using namespace std;
using namespace Tools;

namespace { // anonymous namespace
    void tag_lhead(node *b, node *h, map<node *, node *>& iloopHeaders, map<node *, unsigned>& dfspPos);
    
    node *trav_loops_DFS(node *b0, cfg *g, set<node *> &visited, map<node *, unsigned>& dfspPos, map<node *, node *>& iloopHeaders, unsigned currTime) {
        visited.insert(b0);
        dfspPos[b0] = currTime;
        for(auto& b: g->outEdges[b0]) {
            if(!isContained(visited, b)) {
                // case A, when the next node is not visited
                node *nh = trav_loops_DFS(b, g, visited, dfspPos, iloopHeaders, currTime + 1);
                tag_lhead(b0, nh, iloopHeaders, dfspPos);
            } else {
                if (dfspPos[b] > 0) {
                    // case B
                    tag_lhead(b0, b, iloopHeaders, dfspPos);
                } else if(iloopHeaders[b] == nullptr) {
                    // case C
                    // do nothing 
                } else {
                    node *h = iloopHeaders[b];
                    if(dfspPos[h] > 0) {
                        // case D
                        tag_lhead(b0, h, iloopHeaders, dfspPos);
                        /* cerr << "case D: b(" << b0->name << ") -> h(" << h->name << ")\n"; */
                    } else {
                        // case E
                        /* cerr << "found a irreducible loop at " << h->name << "\n"; */
                        while(iloopHeaders[h]) {
                            h = iloopHeaders[h];
                            if(dfspPos[h] > 0) {
                                tag_lhead(b0, h, iloopHeaders, dfspPos);
                                break;
                            }
                            /* cerr << "found a irreducible loop at " << h->name << "\n"; */
                        }
                    }
                }
            }
        }
        dfspPos[b0] = 0;
        return iloopHeaders[b0];
    }

    void tag_lhead(node *b, node *h, map<node *, node *>& iloopHeaders, map<node *, unsigned>& dfspPos) {
        if(b == h || h == nullptr) {
            return;
        }
        node *cur1 = b, *cur2 = h;
        while(iloopHeaders[cur1] != nullptr) {
            node *ih = iloopHeaders[cur1]; 
            if(ih == cur2) return;
            if(dfspPos[ih] < dfspPos[cur2]) {
                iloopHeaders[cur1] = cur2;
                cur1 = cur2;
                cur2 = ih;
            } else {
                cur1 = ih;
            }
        }
        iloopHeaders[cur1] = cur2;
    }

} // namespace


void fillIloopHeaders(cfg *g, std::map<node *, node *>& iloopHeaders) {
    set<node *> visited{};
    // initialization
    map<node *, unsigned> dfspPos{};
    for(auto& bb: g->getNodes()) {
        dfspPos[bb] = 0; 
        iloopHeaders[bb] = nullptr;
    }
    node *root = g->getEntry();

    trav_loops_DFS(root, g, visited, dfspPos, iloopHeaders, 1);
}

void printHeaders(const map<node *, node *>& iloopHeaders) {
    for(auto& p: iloopHeaders) {
        cerr << p.first->name << " -> " << (p.second == nullptr ? "nullptr" : p.second->name) << endl;
    }
}

loopNode::loopNode(node *entry, set<node *> &nodes) 
    : entry(entry), parent(nullptr), nodes(nodes), children() {
}
// below is for the root node
loopNode::loopNode(set<node *> &nodes) 
    : entry(nullptr), parent(nullptr), nodes(nodes), children() {
}

loopNode::~loopNode() {
    for(auto &c: children) {
        delete c;
    }
}

void loopNode::addParent(loopNode *p) {
    this->parent = p;
    p->children.push_back(this);
    // I don't do it recursively because we are iterating through all the nodes anyway
    
    /* p->nodes.insert(nodes.begin(), nodes.end()); */
    /* if (p->parent != nullptr) { */
        /* p->addParent(p->parent, p, p->nodes); */
    /* } */
}

/* void loopNode::addParent(loopNode *p, loopNode *c, set<node *> &n) { */
/*     c->parent = p; */
/*     p->children.push_back(c); */
/*     /1* p->nodes.insert(n.begin(), n.end()); *1/ */
/*     if (p->parent != nullptr) { */
/*         p->addParent(p->parent, p->nodes); */
/*     } */
/* } */

void loopNode::addRoot(loopNode *r) {
    r->children.push_back(this);
    /* r->nodes.insert(nodes.begin(), nodes.end()); */
}

/* void loopNode::print(string& filename) { */
/*     if(this->entry != nullptr) { */
/*         /1* errs() << "only root node can call print method!\n"; *1/ */
/*         return; */
/*     } */
/*     ofstream file(filename); */
/*     file << "digraph G {\n"; */
/*     file << "root [label=\"root\"];\n"; */
/*     string edges = ""; */
/*     for(auto &c: this->children) { */
/*         edges += "root -> " + c->entry->name + ";\n"; */
/*         edges += c->__print(file); */
/*     } */
/*     file << edges; */
/*     file << "}\n"; */
/*     file.close(); */  
/* } */

void loopNode::print(ofstream& file, const string& funcname) {
    if(this->entry != nullptr) {
        cerr << "only root node can call print method!\n";
        return;
    }
    file << "digraph " << funcname << "{\n"\
        << "root [label=\"root\"];\n";
    string edges = "";
    for(auto &c: this->children) {
        edges += "root  -> " + c->entry->name + ";\n";
        edges += c->__print(file);
    }
    file << edges << "}\n\n";
}

string loopNode::__print(ofstream& file) {
    string edges = "";
    file << entry->name << " [label=\"" << entry->name << "\\l\\n";
    for(auto &n: nodes) {
        file << n->name << "\\l\\n";
    }
    file << "\"];\n";
    for(auto &c: children) {
        edges += entry->name + " -> " + c->entry->name + ";\n";
        edges += c->__print(file);
    }
    return edges;
}

loopNode *createLoopTree(std::map<node *, node *>& iloopHeaders, cfg *g) {
    map<node *, set<node *>> SCCs{}; // key is the entry node
    for(auto &p: iloopHeaders) {
        node *h = p.second;
        node *n = p.first;
        if(h == nullptr) continue;
        if(isContained(SCCs, h)) {
            SCCs[h].insert(n);
        } else {
            SCCs[h] = set<node *>{n};
        }
    }
    // creating loop nodes
    map<node *, loopNode *> loopNodes{}; // key is the nearest loop header
    for(auto &p: SCCs) {
        loopNode *ln = new loopNode(p.first, p.second);
        loopNodes[p.first] = ln;
    }
    // creating loop forest
    for(auto &ln: loopNodes) {
        if(iloopHeaders[ln.first] != nullptr) {
            ln.second->addParent(loopNodes[iloopHeaders[ln.first]]);
        }
    }
    // add the root and make it a tree
    set<node *> allNodes = g->getNodes();
    loopNode *root = new loopNode(allNodes);
    for(auto &ln: loopNodes) {
        if(ln.second->parent == nullptr) {
            ln.second->addRoot(root);
        }
    }
    return root;
}

