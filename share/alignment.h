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

// 前向声明
class ElementPtr;

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

    std::vector<std::vector<ElementPtr>> funcs;
    std::string funcname;
    unsigned long index;
    int id;
    bool isEntry;
    bool isExit;
    bool isLoop = false;
    int loopIndex = -1;
};

// 内存池实现
class ElementPool {
private:
    std::vector<element*> elements; // 改为指针数组
    std::vector<size_t> freeIndices;
    size_t capacity;

public:
    ElementPool(size_t initialCapacity = 10000) : capacity(initialCapacity) {
        elements.reserve(initialCapacity);
    }

    ~ElementPool() {
        // 释放所有分配的内存
        for (auto ptr : elements) {
            if (ptr) delete ptr;
        }
    }

    // 分配一个新元素
    element* allocate(bool isEntry, bool isExit, int id, std::string& funcname, bool isLoop = false) {
        element* ptr = nullptr;

        if (!freeIndices.empty()) {
            size_t index = freeIndices.back();
            freeIndices.pop_back();
            if (elements[index]) {
                delete elements[index]; // 释放原对象
            }
            ptr = new element(isEntry, isExit, id, funcname, isLoop);
            elements[index] = ptr;
        } else {
            ptr = new element(isEntry, isExit, id, funcname, isLoop);
            elements.push_back(ptr);
        }

        return ptr;
    }

    // 释放一个元素
    void deallocate(element* ptr) {
        if (!ptr) return;

        // 寻找元素在池中的位置
        for (size_t i = 0; i < elements.size(); i++) {
            if (elements[i] == ptr) {
                // 不立即删除，只标记为可重用
                freeIndices.push_back(i);
                // 不置空元素，保持有效直到被重新分配
                return;
            }
        }
    }
};

// 改进 ElementPtr 实现
class ElementPtr {
private:
    element* ptr;

public:
    ElementPtr() : ptr(nullptr) {}
    ElementPtr(element* p) : ptr(p) {}
    ElementPtr(const ElementPtr& other) : ptr(other.ptr) {}

    ~ElementPtr() {
        // 由ElementPool管理生命周期
    }

    element* get() const { return ptr; }

    element& operator*() const {
        if (!ptr) {
            fprintf(stderr, "Fatal: Dereferencing null ElementPtr\n");
            abort(); // 安全终止，防止未定义行为
        }
        return *ptr;
    }

    element* operator->() const {
        if (!ptr) {
            fprintf(stderr, "Fatal: Arrow operator on null ElementPtr\n");
            abort(); // 安全终止，防止未定义行为
        }
        return ptr;
    }

    operator bool() const { return ptr != nullptr; }

    ElementPtr& operator=(element* p) {
        ptr = p;
        return *this;
    }

    ElementPtr& operator=(const ElementPtr& other) {
        ptr = other.ptr;
        return *this;
    }

    bool operator==(const ElementPtr& other) const {
        if (!ptr && !other.ptr) return true;
        if (!ptr || !other.ptr) return false;
        return ptr == other.ptr;
    }

    bool operator!=(const ElementPtr& other) const {
        return !(*this == other);
    }
};

// 全局内存池
extern ElementPool elementPool;

// 辅助函数，取代shared_ptr<element>的创建
inline element* createElement(bool isEntry, bool isExit, int id, std::string& funcname, bool isLoop = false) {
    return elementPool.allocate(isEntry, isExit, id, funcname, isLoop);
}

// 拥有索引版本
inline element* createElement(bool isEntry, bool isExit, int id, std::string& funcname, unsigned long index, bool isLoop = false) {
    element* e = elementPool.allocate(isEntry, isExit, id, funcname, isLoop);
    e->index = index;
    return e;
}

// 循环版本
inline element* createElement(int id, std::string& funcname, unsigned long index, bool isLoop = true) {
    element* e = elementPool.allocate(false, false, id, funcname, isLoop);
    e->index = index;
    return e;
}

// 用于释放element
inline void releaseElement(element* e) {
    elementPool.deallocate(e);
}

// 现在包含其他头文件
#include "alignmentUtils.h"

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
        std::vector<ElementPtr>& traces);

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
std::vector<ElementPtr> makeHierarchyMain(
        std::vector<std::vector<std::string>>& traces, unsigned long& index);
std::vector<ElementPtr> makeHierarchy(
        std::vector<std::vector<std::string>>& traces, unsigned long& index);

/*
 * these functions below consider loops
 */
std::vector<ElementPtr> makeHierarchyMain(
        std::vector<std::vector<std::string>>& traces,
        unsigned long &index,
        std::unordered_map<std::string, loopNode *>& loopNodes);
std::vector<ElementPtr> makeHierarchy(
        std::vector<std::vector<std::string>>& traces,
        unsigned long &index,
        std::unordered_map<std::string, loopNode *>& loopNodes);
void addHierarchy(
        std::vector<ElementPtr>& functionalTraces,
        std::vector<std::vector<std::string>>& traces,
        unsigned long &index,
        std::unordered_map<std::string, loopNode *>& loopNodes);

bool greedyAlignmentWholeOffline(
        std::vector<ElementPtr>& original,
        std::vector<ElementPtr>& reproduced,
        const int& rank);
bool greedyAlignmentWholeOffline();

std::deque<std::shared_ptr<lastaligned>> greedyalignmentOnline(
        std::vector<ElementPtr>& original,
        std::vector<ElementPtr>& reproduced,
        std::deque<std::shared_ptr<lastaligned>>& q,
        size_t &i,
        size_t &j,
        const size_t& funcId,
        const int &rank,
        bool& isaligned,
        size_t& lastind,
        ElementPtr originalParent = nullptr,
        ElementPtr reproducedParent = nullptr);

std::vector<std::string> getMsgs(
        std::vector<std::string> &orders,
        const size_t lastind,
        unsigned& order_index,
        std::string *recSendNodes = nullptr);

void appendRecordTracesRaw(std::vector<std::string> rawRecordTrace);

size_t getIndex(ElementPtr& eptr);
std::vector<ElementPtr> getCurrNodesByIndex(unsigned long index);

size_t getIndexFromDeque(std::deque<std::shared_ptr<lastaligned>>& q);

#endif // ALIGNMENT_H
