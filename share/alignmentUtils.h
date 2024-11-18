
#pragma once

#include "alignment.h"

class element;

void insertIterationsAndFixStack(
        std::vector<std::shared_ptr<element>> &iterations,
        std::stack<std::shared_ptr<element>> &stack,
        std::shared_ptr<element> &curr,
        std::shared_ptr<element> &parent,
        std::vector<std::shared_ptr<element>> &functionalTraces);

void insertIterations(
        std::vector<std::shared_ptr<element>> &iterations,
        std::shared_ptr<element> &parent,
        std::vector<std::shared_ptr<element>> &functionalTraces);

void levelUpStack(
        std::stack<std::shared_ptr<element>> &stack,
        std::shared_ptr<element> &curr,
        std::shared_ptr<element> &parent);

void fixIterations(
        std::vector<std::shared_ptr<element>>& functionalTraces,
        std::shared_ptr<element> newChild);
