
#ifndef ALIGNMENT_H
#define ALIGNMENT_H

#include <vector>
#include <memory>
#include <unordered_map>
#include <string>
#include <stack>

#include "tools.h"

class element {
public:
    element() = delete;
    element(const element &e) = default;
    element& operator=(const element &e) = default; 
    ~element() = default;

    /* element(bool isEntry, bool isExit, std::string& bb); */
    /* element(bool isEntry, bool isExit, int id, std::string& funcname); */
    element(bool isEntry, bool isExit, int id, std::string& funcname);

    std::string bb() const;

    std::vector<std::vector<std::shared_ptr<element>>> funcs;
    std::string funcname;
    int id;
    bool isEntry;
    bool isExit;
};

std::vector<std::shared_ptr<element>> makeHierarchyMain(std::vector<std::vector<std::string>>& traces, unsigned long& index);
std::vector<std::shared_ptr<element>> makeHierarchy(std::vector<std::vector<std::string>>& traces, unsigned long& index);
void addHierarchy(std::vector<std::shared_ptr<element>>& functionalTraces, std::vector<std::vector<std::string>>& traces, unsigned long& index);

void print(std::vector<std::shared_ptr<element>>& functionalTraces, unsigned int depth);

void appendReplayTrace();
bool greedyalignmentWhole(std::vector<std::shared_ptr<element>>& original, std::vector<std::shared_ptr<element>>& reproduced, const int& rank);
bool greedyalignmentWhole();


#endif // ALIGNMENT_H
