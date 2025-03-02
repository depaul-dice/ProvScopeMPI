#ifndef MPIRECORDREPLAY_H
#define MPIRECORDREPLAY_H

#include <mpi.h>

/*
 * some functions in C
 */
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cassert>

/*
 * STL data structures
 */
#include <string>
#include <vector>
#include <queue>
#include <map>
#include <unordered_map>
#include <unordered_set>

#include <nlohmann/json.hpp>

/*
 * others
 */
#include <chrono>
#include <memory>
#include <algorithm>

/*
 * libraries that I made
 */
#include "utils.h"
#include "loops.h"
#include "alignment.h"
#include "messagePool.h"
#include "messageTools.h"

// 添加类型转换函数 - 使用inline避免多重定义问题
inline std::vector<std::shared_ptr<element>> convertToSharedPtr(const std::vector<ElementPtr>& input) {
    std::vector<std::shared_ptr<element>> result;
    result.reserve(input.size());

    for (const auto& elem : input) {
        // 获取ElementPtr内部的原始指针，然后创建新的shared_ptr
        element* rawPtr = elem.get();  // 假设ElementPtr有get()方法
        if (rawPtr) {
            std::shared_ptr<element> converted(rawPtr, [](element*){/* 空析构器，防止double-free */});
            result.push_back(converted);
        }
    }

    return result;
}

// 声明并初始化全局变量为空
inline std::vector<ElementPtr> recordTracesElementPtr;
inline std::vector<ElementPtr> replayTracesElementPtr;

// 修改转换函数，使它只是将指针保存到另一个全局变量中
inline void saveElementPtr(const std::vector<ElementPtr>& traces) {
    recordTracesElementPtr = traces;
}

#endif // MPIRECORDREPLAY_H
