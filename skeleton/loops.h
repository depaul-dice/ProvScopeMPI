
#ifndef LOOPS_H
#define LOOPS_H

#include <map>
#include <set>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <cassert>

#include "tools.h"
#include "cfg.h"


void fillIloopHeaders(cfg *g, std::map<node *, node *>& iloopHeaders);
void printHeaders(const std::map<node *, node *>& iloopHeaders);

// hidden functions 

/* node *trav_loops_DFS(node *b0, std::set<node *> &visited, std::map<node *, int>& dfspPos, std::map<node *, node *> iloopHeaders, std::map<node *, std::set<node *>> backedges, unsigned currTime); */

/* void tag_lhead(node *b, node *h, std::map<node *, node *>& iloopHeaders); */


class loopNode {
public:
    loopNode() = delete;
    loopNode(const loopNode&) = delete;
    loopNode& operator = (const loopNode& rhs) = delete; // I will need to deep copy
    ~loopNode();

    loopNode(node *entry, std::set<node *> &nodes);
    loopNode(std::set<node *> &nodes);
    void addParent(loopNode *p);
    void addRoot(loopNode *r);

    /* void print(std::string& file); */
    void print(std::ofstream& file, cont std::string& funcname);

    node *entry;
    loopNode *parent;
    std::set<node *> nodes;
    std::vector<loopNode *> children;

private:
    /* void addParent(loopNode *p, std::set<node *> &n); */
    std::string __print(std::ofstream& file);
};

loopNode *createLoopTree(std::map<node *, node *>& iloopHeaders, cfg *g);
#endif // LOOPS_H
