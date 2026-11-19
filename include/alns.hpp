#ifndef ALNS_HPP
#define ALNS_HPP

#include "problem.hpp"
#include "solution.hpp"
#include "utils.hpp"
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <vector>

struct Operator {
    std::string name;
    double weight      = 1.0;
    double periodScore = 0.0;
    int periodUses     = 0;
    double totalScore  = 0.0;
    int totalUses      = 0;
    int bestFound      = 0;

    Operator() : name("Operator") {}
    explicit Operator(std::string name) : name(std::move(name)) {}
    virtual ~Operator() = default;

    virtual void apply(Solution &sol, const Problem &prob, std::vector<int> &removed, int removeCount, std::mt19937 &rng) = 0;

    void recordUse(double reward, bool isNewBest) noexcept {
        periodUses++;
        totalUses++;
        periodScore += reward;
        totalScore += reward;
        if (isNewBest) bestFound++;
    }

    void print() const {
        std::cerr << "OperatorName = " << name << " | uses = " << totalUses << " | best = " << bestFound << " | score = " << totalScore << " | weight = " << weight << "\n";
    }
};

struct DestroyOperator : public Operator {
    using Operator::Operator;
    DestroyOperator() : Operator("DestroyOperator") {}
};

struct RepairOperator : public Operator {
    using Operator::Operator;
    RepairOperator() : Operator("RepairOperator") {}
};

struct RandomRemoval : public DestroyOperator {
    RandomRemoval() : DestroyOperator("Random Removal") {}
    void apply(Solution &sol, const Problem &prob, std::vector<int> &removed, int removeCount, std::mt19937 &rng) override;
};

struct WorstRemoval : public DestroyOperator {
    WorstRemoval() : DestroyOperator("Worst Removal") {}
    void apply(Solution &sol, const Problem &prob, std::vector<int> &removed, int removeCount, std::mt19937 &rng) override;
};

struct SegmentRemoval : public DestroyOperator {
    SegmentRemoval() : DestroyOperator("Segment Removal") {}
    void apply(Solution &sol, const Problem &prob, std::vector<int> &removed, int removeCount, std::mt19937 &rng) override;
};

struct GreedyInsertion : public RepairOperator {
    GreedyInsertion() : RepairOperator("Greedy Insertion") {}
    void apply(Solution &sol, const Problem &prob, std::vector<int> &removed, int removeCount, std::mt19937 &rng) override;
};

struct TwoRegretInsertion : public RepairOperator {
    TwoRegretInsertion() : RepairOperator("Two Regret Insertion") {}
    void apply(Solution &sol, const Problem &prob, std::vector<int> &removed, int removeCount, std::mt19937 &rng) override;
};

struct ALNS {
    const Problem &prob;
    SolverConfig cfg;
    Solution currentSol;
    Solution bestSol;
    std::mt19937 rng;
    std::string logFilePath;

    std::vector<std::unique_ptr<DestroyOperator>> destroyOperators;
    std::vector<std::unique_ptr<RepairOperator>> repairOperators;

    explicit ALNS(const Problem &prob, const SolverConfig &cfg = SolverConfig{}, std::string logFilePath = "");

    void addDestroy(std::unique_ptr<DestroyOperator> op) {
        destroyOperators.push_back(std::move(op));
    }
    void addRepair(std::unique_ptr<RepairOperator> op) {
        repairOperators.push_back(std::move(op));
    }

    int selectDestroyOperator();
    int selectRepairOperator();
    void updateWeights(double reactionFactor = 0.2);

    Solution solve();
};

Solution solveALNS(const Problem &problem, const SolverConfig &cfg = SolverConfig{}, const std::string &logFilePath = "");

#endif
