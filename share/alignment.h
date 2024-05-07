
#ifndef ALIGNMENT_H
#define ALIGNMENT_H

#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <stack>
#include <limits>
#include <deque>

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
    element(bool isEntry, bool isExit, int id, std::string& funcname, unsigned long index);
    bool operator ==(const element &e) const;

    std::string bb() const;

    std::vector<std::vector<std::shared_ptr<element>>> funcs;
    std::string funcname;
    unsigned long index;
    int id;
    bool isEntry;
    bool isExit;
};

struct lastaligned {
    lastaligned();
    lastaligned(unsigned long funcId, unsigned long origIndex, unsigned long repIndex);
    lastaligned(const lastaligned &l) = default;
    lastaligned& operator=(const lastaligned &l) = default;
    ~lastaligned() = default;

    friend std::ostream& operator<<(std::ostream& os, const lastaligned& l);

    unsigned long funcId;
    unsigned long origIndex;
    unsigned long repIndex;

    bool isSuccess();
};

typedef struct lastaligned lastaligned;

std::deque<std::shared_ptr<lastaligned>> onlineAlignment(std::deque<std::shared_ptr<lastaligned>>& q, bool& isaligned, size_t& lastind);
void appendReplayTrace();

std::vector<std::shared_ptr<element>> makeHierarchyMain(std::vector<std::vector<std::string>>& traces, unsigned long& index);
std::vector<std::shared_ptr<element>> makeHierarchy(std::vector<std::vector<std::string>>& traces, unsigned long& index);
void addHierarchy(std::vector<std::shared_ptr<element>>& functionalTraces, std::vector<std::vector<std::string>>& traces, unsigned long& index);

void print(std::vector<std::shared_ptr<element>>& functionalTraces, unsigned int depth);
void printsurface(std::vector<std::shared_ptr<element>>& functionalTraces);

bool greedyalignmentWholeOffline(std::vector<std::shared_ptr<element>>& original, std::vector<std::shared_ptr<element>>& reproduced, const int& rank);
bool greedyalignmentWholeOffline();

std::deque<std::shared_ptr<lastaligned>> greedyalignmentOnline(std::vector<std::shared_ptr<element>>& original, std::vector<std::shared_ptr<element>>& reproduced, std::deque<std::shared_ptr<lastaligned>>& q, size_t &i, size_t &j, const size_t& funcId, const int &rank, bool& isaligned, size_t& lastind);

std::vector<std::string> getmsgs(std::vector<std::string> &orders, const size_t lastind, unsigned& order_index);
#endif // ALIGNMENT_H
