#include "alns.hpp"
#include "evaluator.hpp"
#include "heuristic.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <numeric>

namespace {

void updatePartialFinish(const Problem &prob, Solution &sol, int startIdx = 0) {
    int m = static_cast<int>(sol.route.size());
    sol.finishTime.resize(m);
    if (m == 0) {
        sol.completionTime = 0;
        sol.feasible = (prob.n == 0);
        return;
    }

    int start = std::max(0, startIdx);
    int64_t currTime = (start == 0) ? 0 : sol.finishTime[start - 1];
    int prevNode = (start == 0) ? 0 : sol.route[start - 1];

    for (int k = start; k < m; k++) {
        int v = sol.route[k];
        int64_t arr = currTime + prob.getTravel(prevNode, v);
        int64_t st = std::max<int64_t>(arr, prob.customers[v].e);
        currTime = st + prob.customers[v].s;
        sol.finishTime[k] = currTime;
        prevNode = v;
    }
    sol.completionTime = sol.finishTime.back();
    sol.feasible = (m == prob.n);
}

void recomputeTotalTravelCost(const Problem &prob, Solution &sol) {
    int64_t cost = 0;
    int prevNode = 0;
    for (int u : sol.route) {
        cost += prob.getTravel(prevNode, u);
        prevNode = u;
    }
    sol.travelCost = cost;
}

struct InsertionCandidate {
    int bestPos = -1;
    int64_t bestScore = std::numeric_limits<int64_t>::max();
    int64_t secondScore = std::numeric_limits<int64_t>::max();
};

InsertionCandidate evaluateInsertionsForNode(const Problem &prob, const Solution &sol, int u, bool preserveSlack) {
    InsertionCandidate res;
    int m = static_cast<int>(sol.route.size());
    int latestStartU = prob.customers[u].l - prob.customers[u].s;

    auto itMax = std::upper_bound(sol.finishTime.begin(), sol.finishTime.end(), static_cast<int64_t>(latestStartU));
    int maxPos = static_cast<int>(itMax - sol.finishTime.begin());

    int64_t earlyBound = std::max<int64_t>(0, static_cast<int64_t>(prob.customers[u].e) - 1200);
    auto itMin = std::lower_bound(sol.finishTime.begin(), sol.finishTime.end(), earlyBound);
    int minPos = std::max(0, static_cast<int>(itMin - sol.finishTime.begin()) - 5);

    auto scanRange = [&](int fromPos, int toPos) {
        for (int pos = toPos; pos >= fromPos; pos--) {
            int prevNode = (pos == 0) ? 0 : sol.route[pos - 1];
            int64_t prevTime = (pos == 0) ? 0 : sol.finishTime[pos - 1];

            int64_t arrivalU = prevTime + prob.getTravel(prevNode, u);
            if (arrivalU > latestStartU) continue;
            int64_t startU = std::max<int64_t>(arrivalU, prob.customers[u].e);
            int64_t finishU = startU + prob.customers[u].s;

            int64_t deltaCost = (pos == m)
                ? prob.getTravel(prevNode, u)
                : (prob.getTravel(prevNode, u) + prob.getTravel(u, sol.route[pos]) - prob.getTravel(prevNode, sol.route[pos]));

            if (!preserveSlack && deltaCost >= res.secondScore) continue;

            int64_t currTime = finishU;
            int currNode = u;
            int64_t pushedDelay = 0;
            bool ok = true;

            for (int k = pos; k < m; k++) {
                int v = sol.route[k];
                int64_t arrV = currTime + prob.getTravel(currNode, v);
                int64_t stV = std::max<int64_t>(arrV, prob.customers[v].e);
                int64_t finV = stV + prob.customers[v].s;
                if (finV > prob.customers[v].l) {
                    ok = false;
                    break;
                }
                currTime = finV;
                currNode = v;
                if (currTime <= sol.finishTime[k]) {
                    pushedDelay = 0;
                    break;
                }
                pushedDelay = currTime - sol.finishTime[k];
            }

            if (!ok) continue;

            int64_t waitU = startU - arrivalU;
            int64_t score = preserveSlack ? (deltaCost + pushedDelay / 2 + waitU / 3) : deltaCost;

            if (score < res.bestScore) {
                res.secondScore = res.bestScore;
                res.bestScore = score;
                res.bestPos = pos;
            } else if (score < res.secondScore) {
                res.secondScore = score;
            }
        }
    };

    scanRange(minPos, maxPos);
    if (res.bestPos == -1 && minPos > 0) {
        scanRange(0, minPos - 1);
    }
    return res;
}

} // namespace

void RandomRemoval::apply(Solution &sol, const Problem &prob, std::vector<int> &removed, int removeCount, std::mt19937 &rng) {
    int m = static_cast<int>(sol.route.size());
    int k = std::min(removeCount, m);
    for (int i = 0; i < k; i++) {
        int idx = static_cast<int>(rng() % sol.route.size());
        removed.push_back(sol.route[idx]);
        sol.route.erase(sol.route.begin() + idx);
    }
    updatePartialFinish(prob, sol, 0);
    recomputeTotalTravelCost(prob, sol);
}

void SegmentRemoval::apply(Solution &sol, const Problem &prob, std::vector<int> &removed, int removeCount, std::mt19937 &rng) {
    int m = static_cast<int>(sol.route.size());
    int k = std::min(removeCount, m);
    if (k <= 0) return;

    int startIdx = (m > k) ? static_cast<int>(rng() % (m - k + 1)) : 0;
    removed.insert(removed.end(), sol.route.begin() + startIdx, sol.route.begin() + startIdx + k);
    sol.route.erase(sol.route.begin() + startIdx, sol.route.begin() + startIdx + k);
    updatePartialFinish(prob, sol, startIdx);
    recomputeTotalTravelCost(prob, sol);
}

void WorstRemoval::apply(Solution &sol, const Problem &prob, std::vector<int> &removed, int removeCount, std::mt19937 &rng) {
    int m = static_cast<int>(sol.route.size());
    int k = std::min(removeCount, m);
    if (k <= 0) return;

    std::vector<std::pair<int64_t, int>> costs;
    costs.reserve(m);
    for (int i = 0; i < m; i++) {
        int u = sol.route[i];
        int prev = (i == 0) ? 0 : sol.route[i - 1];
        int64_t detour = (i == m - 1)
            ? prob.getTravel(prev, u)
            : (prob.getTravel(prev, u) + prob.getTravel(u, sol.route[i + 1]) - prob.getTravel(prev, sol.route[i + 1]));
        int64_t noise = static_cast<int64_t>(rng() % 40);
        costs.push_back({detour + noise, i});
    }
    std::partial_sort(costs.begin(), costs.begin() + k, costs.end(), std::greater<>());

    std::vector<int> indices(k);
    for (int i = 0; i < k; i++) indices[i] = costs[i].second;
    std::sort(indices.rbegin(), indices.rend());

    for (int idx : indices) {
        removed.push_back(sol.route[idx]);
        sol.route.erase(sol.route.begin() + idx);
    }
    updatePartialFinish(prob, sol, indices.back());
    recomputeTotalTravelCost(prob, sol);
}

void GreedyInsertion::apply(Solution &sol, const Problem &prob, std::vector<int> &removed, int removeCount, std::mt19937 &rng) {
    (void)removeCount;
    bool preserveSlack = (rng() % 2 == 0);

    if (rng() % 3 == 0) {
        std::sort(removed.begin(), removed.end(), [&](int a, int b) {
            return prob.customers[a].l < prob.customers[b].l;
        });
    } else if (rng() % 3 == 1) {
        std::shuffle(removed.begin(), removed.end(), rng);
    }

    std::vector<int> uninserted;
    for (int u : removed) {
        InsertionCandidate cand = evaluateInsertionsForNode(prob, sol, u, preserveSlack);
        if (cand.bestPos != -1) {
            sol.route.insert(sol.route.begin() + cand.bestPos, u);
            updatePartialFinish(prob, sol, cand.bestPos);
        } else {
            uninserted.push_back(u);
        }
    }
    recomputeTotalTravelCost(prob, sol);
    removed = std::move(uninserted);
}

void TwoRegretInsertion::apply(Solution &sol, const Problem &prob, std::vector<int> &removed, int removeCount, std::mt19937 &rng) {
    (void)removeCount;
    bool preserveSlack = (rng() % 2 == 0);
    std::vector<int> pool = std::move(removed);
    removed.clear();

    while (!pool.empty()) {
        int bestIdx = -1;
        int bestPos = -1;
        int64_t maxRegret = -1;
        int64_t bestPrimaryScore = std::numeric_limits<int64_t>::max();

        for (int i = 0; i < static_cast<int>(pool.size()); i++) {
            int u = pool[i];
            InsertionCandidate cand = evaluateInsertionsForNode(prob, sol, u, preserveSlack);
            if (cand.bestPos == -1) continue;

            int64_t regret = (cand.secondScore == std::numeric_limits<int64_t>::max())
                ? (1000000LL - cand.bestScore)
                : (cand.secondScore - cand.bestScore);

            if (regret > maxRegret || (regret == maxRegret && cand.bestScore < bestPrimaryScore)) {
                maxRegret = regret;
                bestPrimaryScore = cand.bestScore;
                bestIdx = i;
                bestPos = cand.bestPos;
            }
        }

        if (bestIdx == -1) {
            removed.insert(removed.end(), pool.begin(), pool.end());
            break;
        }

        int chosenU = pool[bestIdx];
        pool.erase(pool.begin() + bestIdx);
        sol.route.insert(sol.route.begin() + bestPos, chosenU);
        updatePartialFinish(prob, sol, bestPos);
    }
    recomputeTotalTravelCost(prob, sol);
}

ALNS::ALNS(const Problem &prob, const SolverConfig &cfg, std::string logPath)
    : prob(prob), cfg(cfg), currentSol(prob.n), bestSol(prob.n), rng(cfg.seed), logFilePath(std::move(logPath)) {
    addDestroy(std::make_unique<RandomRemoval>());
    addDestroy(std::make_unique<WorstRemoval>());
    addDestroy(std::make_unique<SegmentRemoval>());

    addRepair(std::make_unique<GreedyInsertion>());
    addRepair(std::make_unique<TwoRegretInsertion>());
}

int ALNS::selectDestroyOperator() {
    double totalWeight = 0.0;
    for (const auto &op : destroyOperators) totalWeight += op->weight;
    std::uniform_real_distribution<double> dist(0.0, totalWeight);
    double r = dist(rng);
    double acc = 0.0;
    for (int i = 0; i < static_cast<int>(destroyOperators.size()); i++) {
        acc += destroyOperators[i]->weight;
        if (r <= acc) return i;
    }
    return static_cast<int>(destroyOperators.size()) - 1;
}

int ALNS::selectRepairOperator() {
    double totalWeight = 0.0;
    for (const auto &op : repairOperators) totalWeight += op->weight;
    std::uniform_real_distribution<double> dist(0.0, totalWeight);
    double r = dist(rng);
    double acc = 0.0;
    for (int i = 0; i < static_cast<int>(repairOperators.size()); i++) {
        acc += repairOperators[i]->weight;
        if (r <= acc) return i;
    }
    return static_cast<int>(repairOperators.size()) - 1;
}

void ALNS::updateWeights(double reactionFactor) {
    for (auto &op : destroyOperators) {
        if (op->periodUses > 0) {
            op->weight = (1.0 - reactionFactor) * op->weight + reactionFactor * (op->periodScore / op->periodUses);
            op->weight = std::max(0.1, op->weight);
        }
        op->periodScore = 0.0;
        op->periodUses = 0;
    }
    for (auto &op : repairOperators) {
        if (op->periodUses > 0) {
            op->weight = (1.0 - reactionFactor) * op->weight + reactionFactor * (op->periodScore / op->periodUses);
            op->weight = std::max(0.1, op->weight);
        }
        op->periodScore = 0.0;
        op->periodUses = 0;
    }
}

Solution ALNS::solve() {
    Timer timer;
    std::ofstream logOut;
    if (!logFilePath.empty()) {
        logOut.open(logFilePath);
    }

    currentSol = buildInitialSolution(prob, cfg.seed);
    std::vector<int> uninserted;

    if (currentSol.feasible) {
        bestSol = currentSol;
        if (logOut.is_open()) {
            logOut << "#iter = 0 | obj = " << bestSol.travelCost
                   << " | completion = " << bestSol.completionTime
                   << " | time = " << std::fixed << std::setprecision(4) << timer.elapsedSeconds()
                   << "s | op = Initial Heuristic\n";
        }
    } else {
        currentSol.route.clear();
        currentSol.finishTime.clear();
        uninserted.resize(prob.n);
        std::iota(uninserted.begin(), uninserted.end(), 1);
        std::sort(uninserted.begin(), uninserted.end(), [&](int a, int b) {
            return prob.customers[a].l < prob.customers[b].l;
        });
        repairOperators[0]->apply(currentSol, prob, uninserted, 0, rng);
        if (uninserted.empty() && currentSol.feasible) {
            bestSol = currentSol;
        }
        if (logOut.is_open()) {
            logOut << "#iter = 0 | obj = " << (bestSol.feasible ? std::to_string(bestSol.travelCost) : "INFEASIBLE(" + std::to_string(currentSol.route.size()) + "/" + std::to_string(prob.n) + ")")
                   << " | time = " << std::fixed << std::setprecision(4) << timer.elapsedSeconds()
                   << "s | op = Partial Initialization\n";
        }
    }

    int n = prob.n;
    int minRem = std::max(2, static_cast<int>(n * cfg.minDestroyRatio));
    int maxRem = std::max(minRem, std::min(45, static_cast<int>(n * cfg.maxDestroyRatio)));
    std::uniform_int_distribution<int> remDist(minRem, maxRem);
    std::uniform_real_distribution<double> probDist(0.0, 1.0);

    double temp = cfg.initialTemp;
    int iter = 0;

    while (!timer.isTimeUp(cfg.timeLimitSec)) {
        iter++;
        int dIdx = selectDestroyOperator();
        int rIdx = selectRepairOperator();
        int q = remDist(rng);

        Solution candSol = currentSol;
        std::vector<int> candRemoved = uninserted;

        destroyOperators[dIdx]->apply(candSol, prob, candRemoved, q, rng);
        repairOperators[rIdx]->apply(candSol, prob, candRemoved, q, rng);

        double reward = 0.0;
        bool isNewBest = false;

        if (!bestSol.feasible) {
            if (candRemoved.empty()) {
                evaluateSolution(prob, candSol, true);
                if (candSol.feasible) {
                    currentSol = candSol;
                    bestSol = candSol;
                    uninserted.clear();
                    reward = 20.0;
                    isNewBest = true;
                }
            } else if (candRemoved.size() < uninserted.size() ||
                       (candRemoved.size() == uninserted.size() && candSol.travelCost < currentSol.travelCost)) {
                currentSol = std::move(candSol);
                uninserted = std::move(candRemoved);
                reward = 8.0;
            } else if (candRemoved.size() == uninserted.size()) {
                double delta = static_cast<double>(candSol.travelCost - currentSol.travelCost);
                if (probDist(rng) < std::exp(-delta / std::max(1.0, temp))) {
                    currentSol = std::move(candSol);
                    uninserted = std::move(candRemoved);
                    reward = 1.0;
                }
            }
        } else if (candRemoved.empty()) {
            candSol.feasible = true;
            int64_t delta = candSol.travelCost - currentSol.travelCost;

            if (candSol.travelCost < bestSol.travelCost) {
                bestSol = candSol;
                currentSol = std::move(candSol);
                reward = 15.0;
                isNewBest = true;
            } else if (delta < 0) {
                currentSol = std::move(candSol);
                reward = 5.0;
            } else if (probDist(rng) < std::exp(-static_cast<double>(delta) / std::max(1.0, temp))) {
                currentSol = std::move(candSol);
                reward = 1.0;
            }
        }

        destroyOperators[dIdx]->recordUse(reward, isNewBest);
        repairOperators[rIdx]->recordUse(reward, isNewBest);

        if (isNewBest && logOut.is_open()) {
            logOut << "#iter = " << iter
                   << " | obj = " << bestSol.travelCost
                   << " | completion = " << bestSol.completionTime
                   << " | time = " << std::fixed << std::setprecision(4) << timer.elapsedSeconds()
                   << "s | op = " << destroyOperators[dIdx]->name << " + " << repairOperators[rIdx]->name << "\n";
        }

        temp = std::max(1.0, temp * cfg.coolingRate);
        if (iter % 50 == 0) {
            updateWeights(0.2);
        }
    }

    if (bestSol.feasible) {
        evaluateSolution(prob, bestSol, true);
    }

    if (logOut.is_open()) {
        logOut << "\n==================== OPERATOR STATISTICS ====================\n";
        logOut << "Total Iterations = " << iter << " | Total Time = " << std::fixed << std::setprecision(4) << timer.elapsedSeconds() << "s\n";
        logOut << "Final Best Objective = " << (bestSol.feasible ? std::to_string(bestSol.travelCost) : "NOT_FEASIBLE") << "\n\n";

        logOut << "[Destroy Operators]\n";
        logOut << std::left << std::setw(22) << "Name" << std::setw(12) << "Uses" << std::setw(12) << "BestFound" << std::setw(14) << "TotalScore" << std::setw(12) << "FinalWeight" << "\n";
        logOut << std::string(72, '-') << "\n";
        for (const auto &op : destroyOperators) {
            logOut << std::left << std::setw(22) << op->name
                   << std::setw(12) << op->totalUses
                   << std::setw(12) << op->bestFound
                   << std::fixed << std::setprecision(2) << std::setw(14) << op->totalScore
                   << std::fixed << std::setprecision(4) << std::setw(12) << op->weight << "\n";
        }

        logOut << "\n[Repair Operators]\n";
        logOut << std::left << std::setw(22) << "Name" << std::setw(12) << "Uses" << std::setw(12) << "BestFound" << std::setw(14) << "TotalScore" << std::setw(12) << "FinalWeight" << "\n";
        logOut << std::string(72, '-') << "\n";
        for (const auto &op : repairOperators) {
            logOut << std::left << std::setw(22) << op->name
                   << std::setw(12) << op->totalUses
                   << std::setw(12) << op->bestFound
                   << std::fixed << std::setprecision(2) << std::setw(14) << op->totalScore
                   << std::fixed << std::setprecision(4) << std::setw(12) << op->weight << "\n";
        }
        logOut << "=============================================================\n";
    }

    return bestSol;
}

Solution solveALNS(const Problem &problem, const SolverConfig &cfg, const std::string &logFilePath) {
    ALNS alns(problem, cfg, logFilePath);
    return alns.solve();
}
