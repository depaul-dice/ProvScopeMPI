#ifndef MPIRECORDREPLAY_H
#define MPIRECORDREPLAY_H

#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cassert>
#include <string>
#include <vector>
#include <queue>
#include <dlfcn.h>
#include <mpi.h>
#include <algorithm>

#include <map>
#include <unordered_map>
#include <unordered_set>
#include <memory>

#include "tools.h"
#include "loops.h"
#include "alignment.h"
#include "messagePool.h"
#include "messageTools.h"

static void (*original_printBBname) (
    const char *name
) = nullptr;

#endif // MPIRECORDREPLAY_H
