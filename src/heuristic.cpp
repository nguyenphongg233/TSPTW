#include "heuristic.hpp"
#include "evaluator.hpp"
#include "utils.hpp"
#include <algorithm>
#include <limits>
#include <numeric>
#include <random>
#include <vector>

Solution nearestFeasibleNeighbor(const Problem &problem, uint32_t seed) {
    int n = problem.n;
    Solution sol(n);
    std::vector<bool> visited(n + 1, false);
    std::mt19937 rng(seed);

    std::vector<int> urgentOrder(n);
    std::iota(urgentOrder.begin(), urgentOrder.end(), 1);
    std::sort(urgentOrder.begin(), urgentOrder.end(), [&](int a, int b) {
        return (problem.customers[a].l - problem.customers[a].s) <
               (problem.customers[b].l - problem.customers[b].s);
    });

    int curr = 0;
    int64_t currentTime = 0;
    int urgentPtr = 0;

    struct CandInfo {
        int64_t score;
        int64_t finish;
        int node;
        bool operator<(const CandInfo &other) const {
            return score < other.score;
        }
    };
    std::vector<CandInfo> safeCands;
    safeCands.reserve(n);

    for (int step = 0; step < n; step++) {
        while (urgentPtr < n && visited[urgentOrder[urgentPtr]]) {
            urgentPtr++;
        }

        safeCands.clear();
        int fallbackCand = -1;
        int64_t fallbackFinish = std::numeric_limits<int64_t>::max();

        for (int u = 1; u <= n; u++) {
            if (visited[u]) continue;
            int travel = problem.getTravel(curr, u);
            int64_t arrival = currentTime + travel;
            int64_t start = std::max<int64_t>(arrival, problem.customers[u].e);
            int64_t finishedTime = start + problem.customers[u].s;
            if (finishedTime > problem.customers[u].l) continue;

            if (finishedTime < fallbackFinish) {
                fallbackFinish = finishedTime;
                fallbackCand = u;
            }

            bool safe = true;
            int checked = 0;
            for (int idx = urgentPtr; idx < n && checked < 25; idx++) {
                int w = urgentOrder[idx];
                if (visited[w]) continue;
                checked++;
                if (w == u) continue;
                int64_t earliestFinishW = std::max<int64_t>(
                    finishedTime + problem.getTravel(u, w),
                    problem.customers[w].e
                ) + problem.customers[w].s;
                if (earliestFinishW > problem.customers[w].l) {
                    safe = false;
                    break;
                }
            }

            if (safe) {
                int64_t waitTime = start - arrival;
                int64_t score = finishedTime + waitTime;
                safeCands.push_back({score, finishedTime, u});
            }
        }

        int candidate = -1;
        int64_t chosenFinish = -1;

        if (!safeCands.empty()) {
            if (seed == 0 || safeCands.size() == 1) {
                auto bestIt = std::min_element(safeCands.begin(), safeCands.end(), [&](const CandInfo &a, const CandInfo &b) {
                    if (a.score != b.score) return a.score < b.score;
                    return problem.customers[a.node].l < problem.customers[b.node].l;
                });
                candidate = bestIt->node;
                chosenFinish = bestIt->finish;
            } else {
                int k = std::min(3, static_cast<int>(safeCands.size()));
                std::partial_sort(safeCands.begin(), safeCands.begin() + k, safeCands.end());
                std::uniform_int_distribution<int> pickDist(0, k - 1);
                int idx = pickDist(rng);
                candidate = safeCands[idx].node;
                chosenFinish = safeCands[idx].finish;
            }
        } else {
            candidate = fallbackCand;
            chosenFinish = fallbackFinish;
        }

        if (candidate == -1) {
            for (int u = 1; u <= n; u++) {
                if (!visited[u]) {
                    sol.route[step++] = u;
                }
            }
            sol.feasible = false;
            return sol;
        }

        sol.finishTime[step] = chosenFinish;
        sol.route[step] = candidate;
        sol.travelCost += problem.getTravel(curr, candidate);
        currentTime = chosenFinish;
        curr = candidate;
        visited[candidate] = true;
    }

    sol.completionTime = currentTime;
    sol.feasible = true;
    return sol;
}

namespace {

void recomputePrefixFinish(const Problem &problem,
                           const std::vector<int> &route,
                           std::vector<int64_t> &prefixFinish,
                           int startIdx = 0) {
    int m = static_cast<int>(route.size());
    prefixFinish.resize(m);
    int64_t currTime = (startIdx <= 0) ? 0 : prefixFinish[startIdx - 1];
    int prevNode = (startIdx <= 0) ? 0 : route[startIdx - 1];

    for (int k = std::max(0, startIdx); k < m; k++) {
        int v = route[k];
        int64_t arrV = currTime + problem.getTravel(prevNode, v);
        int64_t stV = std::max<int64_t>(arrV, problem.customers[v].e);
        currTime = stV + problem.customers[v].s;
        prefixFinish[k] = currTime;
        prevNode = v;
    }
}

int findBestInsertionPos(const Problem &problem,
                         const std::vector<int> &route,
                         const std::vector<int64_t> &prefixFinish,
                         int u,
                         int mode) {
    int m = static_cast<int>(route.size());
    int bestPos = -1;
    int64_t bestScore = std::numeric_limits<int64_t>::max();
    int64_t bestSecondary = std::numeric_limits<int64_t>::max();

    for (int pos = m; pos >= 0; pos--) {
        int prevNode = (pos == 0) ? 0 : route[pos - 1];
        int64_t prevTime = (pos == 0) ? 0 : prefixFinish[pos - 1];

        int64_t arrivalU = prevTime + problem.getTravel(prevNode, u);
        int64_t startU = std::max<int64_t>(arrivalU, problem.customers[u].e);
        int64_t finishU = startU + problem.customers[u].s;
        if (finishU > problem.customers[u].l) continue;

        int64_t deltaCost = 0;
        if (pos == m) {
            deltaCost = problem.getTravel(prevNode, u);
        } else {
            int nextNode = route[pos];
            deltaCost = problem.getTravel(prevNode, u)
                      + problem.getTravel(u, nextNode)
                      - problem.getTravel(prevNode, nextNode);
        }

        if (mode == 1 && deltaCost > bestScore) continue;

        int64_t currTime = finishU;
        int currNode = u;
        int64_t pushedDelay = 0;
        bool ok = true;

        for (int k = pos; k < m; k++) {
            int v = route[k];
            int64_t arrV = currTime + problem.getTravel(currNode, v);
            int64_t stV = std::max<int64_t>(arrV, problem.customers[v].e);
            int64_t finV = stV + problem.customers[v].s;
            if (finV > problem.customers[v].l) {
                ok = false;
                break;
            }
            currTime = finV;
            currNode = v;

            if (currTime <= prefixFinish[k]) {
                pushedDelay = 0;
                break;
            }
            pushedDelay = currTime - prefixFinish[k];
        }

        if (!ok) continue;

        int64_t waitU = startU - arrivalU;
        int64_t primary = (mode == 0) ? (deltaCost + pushedDelay + waitU / 2) : deltaCost;
        int64_t secondary = finishU;

        if (primary < bestScore || (primary == bestScore && secondary < bestSecondary)) {
            bestScore = primary;
            bestSecondary = secondary;
            bestPos = pos;
        }
    }

    return bestPos;
}

void fastCompressRoute(const Problem &problem,
                       std::vector<int> &route,
                       std::vector<int64_t> &prefixFinish) {
    int m = static_cast<int>(route.size());
    if (m <= 2) return;

    std::vector<std::pair<int64_t, int>> detours;
    detours.reserve(m);
    for (int i = 0; i < m; i++) {
        int u = route[i];
        int prev = (i == 0) ? 0 : route[i - 1];
        int64_t saving = (i == m - 1)
            ? problem.getTravel(prev, u)
            : (problem.getTravel(prev, u) + problem.getTravel(u, route[i + 1]) - problem.getTravel(prev, route[i + 1]));
        if (saving > 20) {
            detours.push_back({saving, u});
        }
    }
    std::sort(detours.rbegin(), detours.rend());
    int limit = std::min(40, static_cast<int>(detours.size()));

    for (int idx = 0; idx < limit; idx++) {
        int u = detours[idx].second;
        auto it = std::find(route.begin(), route.end(), u);
        if (it == route.end()) continue;
        int i = static_cast<int>(it - route.begin());

        int prevI = (i == 0) ? 0 : route[i - 1];
        int64_t oldSaving = (i == m - 1)
            ? problem.getTravel(prevI, u)
            : (problem.getTravel(prevI, u) + problem.getTravel(u, route[i + 1]) - problem.getTravel(prevI, route[i + 1]));

        route.erase(route.begin() + i);
        recomputePrefixFinish(problem, route, prefixFinish, i);

        int pos = findBestInsertionPos(problem, route, prefixFinish, u, 1);
        if (pos == -1) pos = i;

        int prevP = (pos == 0) ? 0 : route[pos - 1];
        int tm = m - 1;
        int64_t newCost = (pos == tm)
            ? problem.getTravel(prevP, u)
            : (problem.getTravel(prevP, u) + problem.getTravel(u, route[pos]) - problem.getTravel(prevP, route[pos]));

        if (newCost > oldSaving) {
            pos = i;
        }
        route.insert(route.begin() + pos, u);
        recomputePrefixFinish(problem, route, prefixFinish, std::min(i, pos));
    }
}

Solution runInsertionWithOrder(const Problem &problem,
                               const std::vector<int> &order,
                               int mode,
                               uint32_t seed = 0) {
    int n = problem.n;
    std::vector<int> route;
    route.reserve(n);
    std::vector<int64_t> prefixFinish;
    prefixFinish.reserve(n);
    std::vector<int> uninserted;

    for (int u : order) {
        int bestPos = findBestInsertionPos(problem, route, prefixFinish, u, mode);
        if (bestPos != -1) {
            route.insert(route.begin() + bestPos, u);
            recomputePrefixFinish(problem, route, prefixFinish, bestPos);
        } else {
            uninserted.push_back(u);
        }
    }

    for (int round = 0; round < 2 && !uninserted.empty(); round++) {
        fastCompressRoute(problem, route, prefixFinish);
        std::vector<int> remaining;
        for (int u : uninserted) {
            int pos = findBestInsertionPos(problem, route, prefixFinish, u, 0);
            if (pos != -1) {
                route.insert(route.begin() + pos, u);
                recomputePrefixFinish(problem, route, prefixFinish, pos);
            } else {
                remaining.push_back(u);
            }
        }
        uninserted = std::move(remaining);
    }

    if (seed > 0 && !uninserted.empty() && route.size() > 5) {
        std::mt19937 rng(seed);
        for (int iter = 0; iter < 30 && !uninserted.empty(); iter++) {
            int targetU = uninserted[rng() % uninserted.size()];
            int targetDeadline = problem.customers[targetU].l;
            int m = static_cast<int>(route.size());

            std::vector<std::pair<int64_t, int>> ejectCandidates;
            for (int i = 0; i < m; i++) {
                int64_t prevTime = (i == 0) ? 0 : prefixFinish[i - 1];
                if (prevTime > targetDeadline) break; 
                int v = route[i];
                int prev = (i == 0) ? 0 : route[i - 1];
                int64_t detour = (i == m - 1)
                    ? problem.getTravel(prev, v)
                    : (problem.getTravel(prev, v) + problem.getTravel(v, route[i + 1]) - problem.getTravel(prev, route[i + 1]));
                int64_t slackV = problem.customers[v].l - prefixFinish[i];
                ejectCandidates.push_back({detour * 3 + slackV / 2, i});
            }

            if (ejectCandidates.empty()) break;
            std::sort(ejectCandidates.rbegin(), ejectCandidates.rend());

            int poolSize = std::min(20, static_cast<int>(ejectCandidates.size()));
            int kickCount = std::min(poolSize, 2 + static_cast<int>(rng() % 5));

            std::vector<int> backupRoute = route;
            std::vector<int64_t> backupFinish = prefixFinish;
            std::vector<int> backupUninserted = uninserted;

            std::vector<int> kickIndices;
            for (int p = 0; p < poolSize; p++) kickIndices.push_back(ejectCandidates[p].second);
            std::shuffle(kickIndices.begin(), kickIndices.end(), rng);
            kickIndices.resize(kickCount);
            std::sort(kickIndices.rbegin(), kickIndices.rend());

            std::vector<int> kickedNodes;
            for (int rmIdx : kickIndices) {
                kickedNodes.push_back(route[rmIdx]);
                route.erase(route.begin() + rmIdx);
            }
            recomputePrefixFinish(problem, route, prefixFinish, kickIndices.back());

            std::vector<int> toInsert = uninserted;
            std::shuffle(toInsert.begin(), toInsert.end(), rng);
            std::sort(kickedNodes.begin(), kickedNodes.end(), [&](int a, int b) {
                return problem.customers[a].l < problem.customers[b].l;
            });
            toInsert.insert(toInsert.end(), kickedNodes.begin(), kickedNodes.end());

            std::vector<int> newUninserted;
            for (int u : toInsert) {
                int pos = findBestInsertionPos(problem, route, prefixFinish, u, 0);
                if (pos != -1) {
                    route.insert(route.begin() + pos, u);
                    recomputePrefixFinish(problem, route, prefixFinish, pos);
                } else {
                    newUninserted.push_back(u);
                }
            }

            if (newUninserted.size() <= backupUninserted.size()) {
                uninserted = std::move(newUninserted);
                if (!uninserted.empty() && iter % 4 == 3) {
                    fastCompressRoute(problem, route, prefixFinish);
                }
            } else {
                route = std::move(backupRoute);
                prefixFinish = std::move(backupFinish);
                uninserted = std::move(backupUninserted);
            }
        }
    }

    if (!uninserted.empty()) {
        Solution failed(n);
        failed.feasible = false;
        return failed;
    }

    Solution sol(n);
    sol.route = std::move(route);
    evaluateSolution(problem, sol);
    return sol;
}

} // namespace

Solution greedyInsertion(const Problem &problem, uint32_t seed) {
    int n = problem.n;
    std::vector<int> baseOrder(n);
    std::iota(baseOrder.begin(), baseOrder.end(), 1);

    if (seed == 0) {
        std::vector<int> orderByL = baseOrder;
        std::sort(orderByL.begin(), orderByL.end(), [&](int a, int b) {
            if (problem.customers[a].l != problem.customers[b].l)
                return problem.customers[a].l < problem.customers[b].l;
            return problem.customers[a].e < problem.customers[b].e;
        });

        std::vector<int> orderByMid = baseOrder;
        std::sort(orderByMid.begin(), orderByMid.end(), [&](int a, int b) {
            int64_t midA = static_cast<int64_t>(problem.customers[a].e) + problem.customers[a].l;
            int64_t midB = static_cast<int64_t>(problem.customers[b].e) + problem.customers[b].l;
            if (midA != midB) return midA < midB;
            return problem.customers[a].l < problem.customers[b].l;
        });

        Solution bestSol = runInsertionWithOrder(problem, orderByL, 0, 0);

        Solution midSol = runInsertionWithOrder(problem, orderByMid, 0, 0);
        if (midSol.feasible && (!bestSol.feasible || midSol.travelCost < bestSol.travelCost)) {
            bestSol = std::move(midSol);
        }
        return bestSol;
    }

    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> noiseDist(-150, 150);
    std::vector<std::pair<int64_t, int>> keyedOrder(n);

    int strategy = static_cast<int>(seed % 3);
    for (int i = 0; i < n; i++) {
        int u = i + 1;
        int64_t key = 0;
        if (strategy == 0) {
            key = problem.customers[u].l + noiseDist(rng);
        } else if (strategy == 1) {
            key = (static_cast<int64_t>(problem.customers[u].e) + problem.customers[u].l) / 2 + noiseDist(rng);
        } else {
            key = problem.customers[u].e + problem.customers[u].s + noiseDist(rng);
        }
        keyedOrder[i] = {key, u};
    }
    std::sort(keyedOrder.begin(), keyedOrder.end());

    std::vector<int> randOrder(n);
    for (int i = 0; i < n; i++) randOrder[i] = keyedOrder[i].second;

    return runInsertionWithOrder(problem, randOrder, 0, seed);
}

Solution buildInitialSolution(const Problem &problem, uint32_t baseSeed) {
    Solution bestSol = nearestFeasibleNeighbor(problem, 0);

    Solution insSol = greedyInsertion(problem, 0);
    if (insSol.feasible && (!bestSol.feasible || insSol.travelCost < bestSol.travelCost)) {
        bestSol = std::move(insSol);
    }

    int id = 0;
    Timer t0;
    std::mt19937 seedGen(baseSeed);

    while (id < 500 && !t0.isTimeUp(2.0) && !bestSol.feasible) {
        id++;
        uint32_t currentSeed = seedGen();
        if (currentSeed == 0) currentSeed = static_cast<uint32_t>(id);

        Solution candNFN = nearestFeasibleNeighbor(problem, currentSeed);
        if (candNFN.feasible && (!bestSol.feasible || candNFN.travelCost < bestSol.travelCost)) {
            bestSol = std::move(candNFN);
            break;
        }

        Solution candIns = greedyInsertion(problem, currentSeed);
        if (candIns.feasible && (!bestSol.feasible || candIns.travelCost < bestSol.travelCost)) {
            bestSol = std::move(candIns);
            break;
        }
    }

    return bestSol;
}
