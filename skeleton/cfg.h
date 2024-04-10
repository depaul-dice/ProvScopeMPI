#ifndef CFG_H
#define CFG_H

#include <set>
#include <map>
#include <string>
#include <vector>
#include <ctime>
#include <algorithm>

#include "llvm/Support/raw_ostream.h"

class node;

class cfg {
public:
    cfg() = delete;
    cfg(const std::string& funcname);
    cfg(const cfg&) = default;
    cfg& operator=(const cfg&) = default;
    ~cfg();
    
    std::set<node *> findMinimalNodes();
    void insertEdge(const std::string& from, const std::string& to);
    void insertNode(const std::string& name);
    
    std::string funcname;
    std::map<std::string, node *> nodes;
    std::map<node *, std::set<node *>> outEdges;
    std::map<node *, std::set<node *>> inEdges;
};

class node {
public:
    node() = delete;
    node(const std::string& name);
    node(const node&) = default;
    node& operator=(const node&) = default;
    ~node() = default;
    std::string name;
};

#endif // CFG_H
