
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
#include <sstream>

#include "utils.h"
#include "loops.h"
#include "alignmentUtils.h"

enum class TraceType {
    RECORD = 0,
    REPLAY = 1
};

class element {
public:
    element() = delete;
    element(const element &e) = default;
    element& operator=(const element &e) = default; 
    ~element() = default;

    element(
            bool isEntry, 
            bool isExit, 
            int id, 
            std::string& funcname, 
            bool isLoop = false);
    element(
            bool isEntry, 
            bool isExit, 
            int id, 
            std::string& funcname, 
            unsigned long index, 
            bool isLoop = false);
    /*
     * this is for the loop node, index is the iteration cnt of the loop
     */
    element(
            int id, 
            std::string& funcname, 
            unsigned long index, 
            bool isLoop = true);

    bool operator == (const element &e) const;

    friend std::ostream& operator<<(
            std::ostream& os, const element& e);

    std::string bb() const;
    std::string content() const;

    std::vector<std::vector<std::shared_ptr<element>>> funcs;
    std::string funcname;
    unsigned long index;
    int id;
    bool isEntry;
    bool isExit;
    bool isLoop = false;
    int loopIndex = -1;
};

struct lastaligned {
    lastaligned();
    lastaligned(
            unsigned long funcId, 
            unsigned long origIndex, 
            unsigned long repIndex);
    lastaligned(const lastaligned &l) = default;
    lastaligned& operator=(const lastaligned &l) = default;
    ~lastaligned() = default;

    friend std::ostream& operator<<(
            std::ostream& os, const lastaligned& l);

    unsigned long funcId;
    unsigned long origIndex;
    unsigned long repIndex;

    bool isSuccess();
};

typedef struct lastaligned lastaligned;

std::deque<std::shared_ptr<lastaligned>> onlineAlignment(
        std::deque<std::shared_ptr<lastaligned>>& q, 
        bool& isaligned, 
        size_t& lastind, 
        std::unordered_map<std::string, loopNode *>& loopTrees);

void appendReplayTrace(
        std::unordered_map<std::string, loopNode *>& loopTrees);

void appendTraces(
        std::unordered_map<std::string, loopNode *>& loopTrees,
        std::vector<std::vector<std::string>>& rawTraces,
        std::vector<std::shared_ptr<element>>& traces);
 
void appendTraces(
        std::unordered_map<std::string, loopNode *>& loopTrees,
        TraceType traceType);
 
std::string getLastNodes(TraceType traceType);
std::string getVeryLastNode(TraceType traceType = TraceType::RECORD);
std::string updateAndGetLastNodes(
        std::unordered_map<std::string, loopNode *>& loopTrees, 
        TraceType traceType);

/*
 * these functions below DO NOT consider loops at all
 */
std::vector<std::shared_ptr<element>> makeHierarchyMain(
        std::vector<std::vector<std::string>>& traces, unsigned long& index);
std::vector<std::shared_ptr<element>> makeHierarchy(
        std::vector<std::vector<std::string>>& traces, unsigned long& index);

/*
 * these functions below consider loops
 */
std::vector<std::shared_ptr<element>> makeHierarchyMain(
        std::vector<std::vector<std::string>>& traces, 
        unsigned long &index, 
        std::unordered_map<std::string, loopNode *>& loopNodes);
std::vector<std::shared_ptr<element>> makeHierarchy(
        std::vector<std::vector<std::string>>& traces, 
        unsigned long &index, 
        std::unordered_map<std::string, loopNode *>& loopNodes);
void addHierarchy(
        std::vector<std::shared_ptr<element>>& functionalTraces, 
        std::vector<std::vector<std::string>>& traces, 
        unsigned long &index, 
        std::unordered_map<std::string, loopNode *>& loopNodes);


void print(
        std::vector<std::shared_ptr<element>>& functionalTraces, unsigned int depth);
void printsurface(std::vector<std::shared_ptr<element>>& functionalTraces);

bool greedyAlignmentWholeOffline(
        std::vector<std::shared_ptr<element>>& original, 
        std::vector<std::shared_ptr<element>>& reproduced, 
        const int& rank);
bool greedyAlignmentWholeOffline();

std::deque<std::shared_ptr<lastaligned>> greedyalignmentOnline(
        std::vector<std::shared_ptr<element>>& original, 
        std::vector<std::shared_ptr<element>>& reproduced, 
        std::deque<std::shared_ptr<lastaligned>>& q, 
        size_t &i, 
        size_t &j, 
        const size_t& funcId, 
        const int &rank, 
        bool& isaligned, 
        size_t& lastind,
        std::shared_ptr<element> originalParent = nullptr,
        std::shared_ptr<element> reproducedParent = nullptr);

std::vector<std::string> getMsgs(
        std::vector<std::string> &orders, 
        const size_t lastind, 
        unsigned& order_index,
        std::string *recSendNodes = nullptr);

void appendRecordTracesRaw(std::vector<std::string> rawRecordTrace);

size_t getIndex(std::shared_ptr<element>& eptr);
std::vector<std::shared_ptr<element>> getCurrNodesByIndex(unsigned long index);

size_t getIndexFromDeque(std::deque<std::shared_ptr<lastaligned>>& q);

#endif // ALIGNMENT_H
