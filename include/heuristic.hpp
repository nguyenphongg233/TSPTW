#ifndef HEURISTIC_HPP
#define HEURISTIC_HPP

#include "problem.hpp"
#include "solution.hpp"
#include <cstdint>

Solution nearestFeasibleNeighbor(const Problem &problem, uint32_t seed = 0);
Solution greedyInsertion(const Problem &problem, uint32_t seed = 0);
Solution buildInitialSolution(const Problem &problem, uint32_t baseSeed = 42);

#endif
