
#ifndef ALIGNMENT_H
#define ALIGNMENT_H

#include <vector>
#include <memory>

#include "tools.h"

class element {
public:
    element() = delete;
    element(const element &e) = default;
    element& operator=(const element &e) = default; 
    ~element() = default;

    element(bool isEntry, bool isExit, std::string& bb);

    bool isEntry;
    bool isExit;
    std::string bb; // name of the basic block
    std::vector<std::vector<std::shared_ptr<element>>> funcs;
};

std::vector<std::shared_ptr<element>> makeHierarchyWhole(std::vector<std::vector<std::string>>& traces, unsigned long& index);

void print(std::vector<std::shared_ptr<element>>& functionalTraces, unsigned int depth);
void appendReplayTrace();
#endif // ALIGNMENT_H
