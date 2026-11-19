#ifndef EVALUATOR_HPP
#define EVALUATOR_HPP

#include "problem.hpp"
#include "solution.hpp"
#include <cstdint>
#include <vector>
#include <algorithm>

struct EvalResult {
    bool feasible = false;
    int64_t travelCost = 0;
    int64_t completionTime = 0;
    int firstViolatedIndex = -1;

    EvalResult() = default;
    EvalResult(bool f, int64_t t, int64_t c, int fV) : feasible(f), travelCost(t), completionTime(c), firstViolatedIndex(fV){

    }
};

bool isValidPermutation(const Problem& problem, const std::vector<int>& route);

EvalResult evaluateRoute(const Problem& problem,
                            const std::vector<int>& route,
                            bool checkPermutation = true,
                            std::vector<int64_t>* fillFinishTime = nullptr);

bool evaluateSolution(const Problem& problem, Solution& sol, bool checkPermutation = true);
bool evaluateSuffix(const Problem& problem, Solution& sol, int startIdx);

#endif