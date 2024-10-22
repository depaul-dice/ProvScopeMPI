
#ifndef LOOPS_H
#define LOOPS_H

#include <map>
#include <set>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <cassert>

// these are for reading the dotfiles and making the loop trees per function
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <graphviz/cgraph.h>

#include "utils.h"


class loopNode {
public:
    loopNode() = delete;
    loopNode(const loopNode&) = delete;
    loopNode& operator = (const loopNode& rhs) = delete; // I will need to deep copy
    ~loopNode();

    /* loopNode(node *entry, std::set<node *> &nodes); */
    loopNode(
            std::string &entry, std::unordered_set<std::string> &nodes);
    void addChild(loopNode *c);
    void addParent(loopNode *p);
    void addRoot(loopNode *r);
    std::unordered_set<std::string> fixExclusives(void); // you only call this once, after the tree is built

    /* void print(std::string& file); */
    void print(
            std::ofstream& file, const std::string& funcname);
    void print(
            std::ostream& os, const std::string& funcname = "main");
    friend std::ostream& operator << (
            std::ostream& os, loopNode& ln);

    std::string entry; // entry node of the loop
    loopNode *parent;
    std::unordered_set<std::string> nodes; // nodes in the loop
    std::unordered_set<std::string> exclusives; // nodes particularly in this loop
    std::unordered_set<loopNode *> children;

private:
    /* void addParent(loopNode *p, std::set<node *> &n); */
    std::string __print(std::ofstream& file);
    std::string __print(std::ostream& os);
};

void printEdgeInfo(
        Agnode_t* node, Agraph_t* subgraph);
void printNodeInfo(
        Agnode_t* node, Agraph_t* subgraph);

/*
 * this should create one loop tree
 */
std::pair<std::string, loopNode *> parseGraph(
        Agraph_t* subgraph, const std::string& prefix);
/*
 * this should create multiple loop trees
 */
std::unordered_map<std::string, loopNode *> parseDotFile(const std::string& filename);

// at the end of the day, we want to get the map, where key is the function name and the value is the root node (loopNode)
#endif // LOOPS_H
