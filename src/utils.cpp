#include "utils.hpp"
#include "evaluator.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <vector>

Problem generateTestCase(int n, const TestGenConfig& cfg, Solution* outPlantedSol) {
    // 1. Assign random coord, random serving time for each customer
    // 2. Generate semi-greedy random route (plantedRoute)
    // 3. Calculate feasible [e, l] around plantedRoute

    Problem p(n);
    if (n <= 0) return p;

    std::mt19937 rng(cfg.seed);
    std::uniform_int_distribution<int> coordDist(0, std::max(1, cfg.maxCoord));
    std::uniform_int_distribution<int> serviceDist(std::max(0, cfg.minService),
                                                   std::max(cfg.minService, cfg.maxService));
    std::uniform_int_distribution<int> slackDist(0, std::max(0, cfg.windowSlack));
    std::uniform_int_distribution<int> waitDist(0, std::max(0, cfg.maxWaitTime));
    std::uniform_int_distribution<int> pctDist(1, 100);

    std::vector<int> x(n + 1), y(n + 1);
    for (int i = 0; i <= n; i++) {
        x[i] = coordDist(rng);
        y[i] = coordDist(rng);
    }

    for (int i = 0; i <= n; i++) {
        for (int j = 0; j <= n; j++) {
            if (i == j) {
                p.travel[static_cast<size_t>(i) * (n + 1) + j] = 0;
            } else {
                double dx = static_cast<double>(x[i] - x[j]);
                double dy = static_cast<double>(y[i] - y[j]);
                int dist = static_cast<int>(std::hypot(dx, dy)) + 1;
                p.travel[static_cast<size_t>(i) * (n + 1) + j] = dist;
            }
        }
    }

    p.customers[0] = Customer(0, 0, 0);
    for (int i = 1; i <= n; i++) {
        p.customers[i].s = serviceDist(rng);
    }

    std::vector<int> plantedRoute;
    plantedRoute.reserve(n);
    std::vector<bool> visited(n + 1, false);
    int curr = 0;

    int topK = std::max(1, cfg.nearestK);
    for (int step = 0; step < n; step++) {
        std::vector<std::pair<int, int>> candidates;
        candidates.reserve(n - step);
        for (int u = 1; u <= n; u++) {
            if (!visited[u]) {
                candidates.push_back({p.getTravel(curr, u), u});
            }
        }

        int k = std::min(topK, static_cast<int>(candidates.size()));
        std::partial_sort(candidates.begin(), candidates.begin() + k, candidates.end());
        std::uniform_int_distribution<int> pickDist(0, k - 1);
        int chosen = candidates[pickDist(rng)].second;

        visited[chosen] = true;
        plantedRoute.push_back(chosen);
        curr = chosen;
    }

    int64_t currentTime = 0;
    int prev = 0;
    for (int u : plantedRoute) {
        int64_t arrival = currentTime + p.getTravel(prev, u);

        int64_t startService = arrival;
        if (pctDist(rng) <= cfg.waitChancePct) {
            startService = arrival + waitDist(rng);
            p.customers[u].e = static_cast<int>(startService);
        } else {
            int earlySlack = slackDist(rng);
            p.customers[u].e = static_cast<int>(std::max<int64_t>(0, arrival - earlySlack));
        }

        int64_t finishService = startService + p.customers[u].s;
        int lateSlack = slackDist(rng);
        p.customers[u].l = static_cast<int>(finishService + lateSlack);

        currentTime = finishService;
        prev = u;
    }

    if (outPlantedSol != nullptr) {
        *outPlantedSol = Solution(n);
        outPlantedSol->route = std::move(plantedRoute);
        evaluateSolution(p, *outPlantedSol);
    }

    return p;
}

bool saveProblemToFile(const Problem& problem, const std::string& filename) {
    std::ofstream out(filename);
    if (!out.is_open()) return false;

    out << problem.n << "\n";
    for (int i = 1; i <= problem.n; i++) {
        out << problem.customers[i].e << " "
            << problem.customers[i].l << " "
            << problem.customers[i].s << "\n";
    }

    for (int i = 0; i <= problem.n; i++) {
        for (int j = 0; j <= problem.n; j++) {
            out << problem.getTravel(i, j) << (j < problem.n ? " " : "");
        }
        out << "\n";
    }

    return out.good();
}

bool generateDataset(const std::string& outputDir) {
    std::filesystem::create_directories(outputDir);

    std::ofstream refOut(outputDir + "/reference_costs.csv");
    if (!refOut.is_open()) return false;
    refOut << "filename,n,type,window_slack,seed,planted_travel_cost,planted_completion_time\n";

    auto formatIndex = [](int idx) {
        std::ostringstream oss;
        oss << std::setw(2) << std::setfill('0') << idx;
        return oss.str();
    };

    int totalFiles = 0;

    const std::vector<int> smallSizes = {10, 20, 50};
    for (int n : smallSizes) {
        for (int idx = 1; idx <= 10; idx++) {
            TestGenConfig cfg;
            cfg.seed = static_cast<uint32_t>(1000 * n + idx);
            if (idx <= 3) {
                cfg.windowSlack = 50;
                cfg.waitChancePct = 30;
            } else if (idx <= 7) {
                cfg.windowSlack = 250;
                cfg.waitChancePct = 20;
            } else {
                cfg.windowSlack = 1000;
                cfg.waitChancePct = 10;
            }

            std::string fname = "n" + std::to_string(n) + "_" + formatIndex(idx) + ".inp";
            std::string fullPath = outputDir + "/" + fname;

            Solution planted(n);
            Problem p = generateTestCase(n, cfg, &planted);
            if (!saveProblemToFile(p, fullPath)) return false;

            refOut << fname << "," << n << ",small," << cfg.windowSlack << ","
                   << cfg.seed << "," << planted.travelCost << "," << planted.completionTime << "\n";
            totalFiles++;
        }
    }

    struct Profile {
        std::string name;
        int slack;
        int waitPct;
        uint32_t seedOffset;
    };
    const std::vector<Profile> profiles = {
        {"tight", 60,   25, 100},
        {"med",   350,  20, 200},
        {"wide",  2000, 10, 300}
    };
    const std::vector<int> largeSizes = {200, 500, 1000};

    for (int n : largeSizes) {
        for (const auto& prof : profiles) {
            for (int idx = 1; idx <= 5; idx++) {
                TestGenConfig cfg;
                cfg.seed = static_cast<uint32_t>(n * 1000 + prof.seedOffset + idx);
                cfg.windowSlack = prof.slack;
                cfg.waitChancePct = prof.waitPct;

                std::string fname = "n" + std::to_string(n) + "_" + prof.name + "_" + formatIndex(idx) + ".inp";
                std::string fullPath = outputDir + "/" + fname;

                Solution planted(n);
                Problem p = generateTestCase(n, cfg, &planted);
                if (!saveProblemToFile(p, fullPath)) return false;

                refOut << fname << "," << n << "," << prof.name << "," << cfg.windowSlack << ","
                       << cfg.seed << "," << planted.travelCost << "," << planted.completionTime << "\n";
                totalFiles++;
            }
        }
    }

    std::cout << "[Dataset] Generated " << totalFiles << " testcases in '" << outputDir
              << "' (reference saved to " << outputDir << "/reference_costs.csv)\n";
    return true;
}
