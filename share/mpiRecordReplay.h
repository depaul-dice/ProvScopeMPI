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

#endif // MPIRECORDREPLAY_H
