#include "alns.hpp"
#include "evaluator.hpp"
#include "heuristic.hpp"
#include "problem.hpp"
#include "solution.hpp"
#include "utils.hpp"
#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Giải 1 bài toán theo tên thuật toán ("heuristic" hoặc "alns") và giới hạn thời gian
Solution solveInstance(const Problem& problem,
                       const std::string& solverName = "heuristic",
                       const std::string& logPath = "",
                       double customTimeLimit = -1.0) {
    if (solverName == "heuristic") {
        return buildInitialSolution(problem);
    }
    SolverConfig cfg;
    if (customTimeLimit > 0.0) {
        cfg.timeLimitSec = customTimeLimit;
    } else if (problem.n <= 50) {
        cfg.timeLimitSec = 0.5;
    } else if (problem.n <= 200) {
        cfg.timeLimitSec = 2.0;
    } else {
        cfg.timeLimitSec = 5.0;
    }
    return solveALNS(problem, cfg, logPath);
}

void benchmark(const std::string& mode = "all",
               const std::string& solverName = "heuristic",
               double customTimeLimit = -1.0,
               int numThreads = 0) {
    std::string inDir = "data/input/";
    std::string outDir = "data/output/" + solverName + "/";
    std::string resDir = "results/";
    std::string logDir = "results/logs/";

    std::filesystem::create_directories(outDir);
    std::filesystem::create_directories(resDir);
    if (solverName == "alns") {
        std::filesystem::create_directories(logDir);
    }

    // Small testcases:
    std::vector<int> nC = {10, 20, 50};
    std::vector<std::string> nT = {"01", "02", "03", "04", "05", "06", "07", "08", "09", "10"};

    // Big testcases:
    std::vector<int> bnC = {200, 500, 1000};
    std::vector<std::string> type = {"tight", "med", "wide"};
    std::vector<std::string> bnT = {"01", "02", "03", "04", "05"};

    struct BenchCase {
        std::string name;
        std::string group;
    };
    std::vector<BenchCase> cases;

    if (mode == "all" || mode == "small") {
        for (int n : nC) {
            for (const auto& id : nT) {
                cases.push_back({"n" + std::to_string(n) + "_" + id, "small"});
            }
        }
    }

    if (mode == "all" || mode == "big") {
        for (int n : bnC) {
            for (const auto& tp : type) {
                for (const auto& id : bnT) {
                    cases.push_back({"n" + std::to_string(n) + "_" + tp + "_" + id, tp});
                }
            }
        }
    }

    if (numThreads <= 0) {
        int hw = static_cast<int>(std::thread::hardware_concurrency());
        numThreads = (hw > 0) ? hw : 4;
    }
    numThreads = std::min(numThreads, std::max(1, static_cast<int>(cases.size())));

    std::cout << "[Benchmark] Solver: " << solverName
              << " | Mode: " << mode
              << " | Threads: " << numThreads;
    if (customTimeLimit > 0.0) {
        std::cout << " | TimeLimit: " << customTimeLimit << "s";
    }
    std::cout << "\n";
    std::cout << std::left
              << std::setw(22) << "Instance"
              << std::setw(8)  << "N"
              << std::setw(10) << "Group"
              << std::setw(10) << "Feasible"
              << std::setw(14) << "TravelCost"
              << std::setw(16) << "CompletionTime"
              << std::setw(12) << "Time(s)" << "\n";
    std::cout << std::string(92, '-') << "\n";

    struct CaseResult {
        bool loaded = false;
        int n = 0;
        bool feasible = false;
        int64_t travelCost = 0;
        int64_t completionTime = 0;
        double elapsed = 0.0;
    };

    std::vector<CaseResult> results(cases.size());
    std::atomic<size_t> nextIdx{0};
    std::mutex printMutex;
    Timer wallTimer;

    auto worker = [&]() {
        while (true) {
            size_t idx = nextIdx.fetch_add(1);
            if (idx >= cases.size()) break;

            const auto& tc = cases[idx];
            std::string inPath = inDir + tc.name + ".inp";
            std::string outPath = outDir + tc.name + ".out";

            Problem problem;
            if (!problem.loadFromFile(inPath)) {
                std::lock_guard<std::mutex> lock(printMutex);
                std::cerr << "[Skip] Cannot load " << inPath << " (run --gen first)\n";
                continue;
            }

            Timer timer;
            std::string logPath = (solverName == "alns") ? (logDir + tc.name + ".log") : "";
            Solution sol = solveInstance(problem, solverName, logPath, customTimeLimit);
            double elapsed = timer.elapsedSeconds();

            evaluateSolution(problem, sol, true);

            if (sol.feasible) {
                std::ofstream outFile(outPath);
                if (outFile.is_open()) {
                    sol.print(outFile);
                }
            }

            results[idx] = {true, problem.n, sol.feasible, sol.travelCost, sol.completionTime, elapsed};

            {
                std::lock_guard<std::mutex> lock(printMutex);
                std::cout << std::left
                          << std::setw(22) << tc.name
                          << std::setw(8)  << problem.n
                          << std::setw(10) << tc.group
                          << std::setw(10) << (sol.feasible ? "YES" : "NO")
                          << std::setw(14) << (sol.feasible ? std::to_string(sol.travelCost) : "-")
                          << std::setw(16) << (sol.feasible ? std::to_string(sol.completionTime) : "-")
                          << std::fixed << std::setprecision(4) << std::setw(12) << elapsed << "\n";
            }
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(numThreads);
    for (int t = 0; t < numThreads; t++) {
        threads.emplace_back(worker);
    }
    for (auto& th : threads) {
        th.join();
    }

    double wallElapsed = wallTimer.elapsedSeconds();

    // Ghi file CSV theo đúng thứ tự chuẩn của cases
    std::string csvPath = resDir + solverName + "_benchmark_results.csv";
    std::ofstream csvOut(csvPath);
    csvOut << "instance,n,group,feasible,travel_cost,completion_time,runtime_sec\n";

    int totalCases = 0;
    int feasibleCases = 0;
    int64_t sumFeasibleCost = 0;
    double totalCpuTime = 0.0;

    for (size_t i = 0; i < cases.size(); i++) {
        const auto& tc = cases[i];
        const auto& r = results[i];
        if (!r.loaded) continue;

        totalCases++;
        totalCpuTime += r.elapsed;
        if (r.feasible) {
            feasibleCases++;
            sumFeasibleCost += r.travelCost;
        }

        csvOut << tc.name << ","
               << r.n << ","
               << tc.group << ","
               << (r.feasible ? 1 : 0) << ","
               << r.travelCost << ","
               << r.completionTime << ","
               << std::fixed << std::setprecision(6) << r.elapsed << "\n";
    }

    std::cout << std::string(92, '-') << "\n";
    std::cout << "[Summary] Feasible: " << feasibleCases << "/" << totalCases
              << " | Avg Feasible Cost: " << (feasibleCases > 0 ? sumFeasibleCost / feasibleCases : 0)
              << " | Wall Time: " << std::fixed << std::setprecision(4) << wallElapsed
              << "s (CPU sum: " << totalCpuTime << "s)\n";
    std::cout << "[Saved]   Routes  -> '" << outDir << "'\n";
    std::cout << "[Saved]   CSV     -> '" << csvPath << "'\n";
}

int main(int argc, char* argv[]) {
    // Tách cờ -t / --time <seconds> và -j / --threads <num_threads>
    double customTimeLimit = -1.0;
    int numThreads = 0;
    std::vector<std::string> args;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if ((a == "-t" || a == "--time") && i + 1 < argc) {
            customTimeLimit = std::stod(argv[++i]);
        } else if ((a == "-j" || a == "--threads") && i + 1 < argc) {
            numThreads = std::stoi(argv[++i]);
        } else {
            args.push_back(a);
        }
    }

    if (!args.empty() && args[0] == "--gen") {
        std::string outDir = (args.size() >= 2) ? args[1] : "data/input";
        Timer timer;
        if (!generateDataset(outDir)) {
            std::cerr << "Error: Failed to generate dataset in " << outDir << "\n";
            return 1;
        }
        std::cout << "[Dataset] Done in " << timer.elapsedSeconds() << "s\n";
        return 0;
    }

    if (!args.empty() && args[0] == "--bench") {
        std::string mode = (args.size() >= 2) ? args[1] : "all";
        std::string solverName = (args.size() >= 3) ? args[2] : "heuristic";
        if (args.size() >= 4 && customTimeLimit < 0.0) {
            customTimeLimit = std::stod(args[3]);
        }
        benchmark(mode, solverName, customTimeLimit, numThreads);
        return 0;
    }

    if (args.empty()) {
        std::cerr << "Usage:\n"
                  << "  1. Generate dataset: " << argv[0] << " --gen [output_dir]\n"
                  << "  2. Run benchmark:    " << argv[0] << " --bench [all|small|big] [heuristic|alns] [-t <sec>] [-j <threads>]\n"
                  << "  3. Solve instance:   " << argv[0] << " <input.inp> [output.out] [heuristic|alns] [-t <sec>]\n";
        return 1;
    }

    std::string inputFile = args[0];
    std::string outputFile = (args.size() >= 2 && args[1] != "heuristic" && args[1] != "alns") ? args[1] : "";
    std::string solverName = "heuristic";
    for (size_t i = 1; i < args.size(); i++) {
        if (args[i] == "heuristic" || args[i] == "alns") {
            solverName = args[i];
        }
    }

    Timer timer;
    Problem problem;
    if (!problem.loadFromFile(inputFile)) {
        std::cerr << "Error: Cannot load input file '" << inputFile << "'\n";
        return 1;
    }
    std::cerr << "[Info] Loaded N = " << problem.n << " from '" << inputFile
              << "' in " << timer.elapsedSeconds() << "s\n";

    std::filesystem::create_directories("results/logs");
    std::string singleLog = (solverName == "alns") ? "results/logs/last_run.log" : "";
    Solution sol = solveInstance(problem, solverName, singleLog, customTimeLimit);
    sol.debug();

    if (!outputFile.empty()) {
        std::ofstream outFile(outputFile);
        if (!outFile.is_open()) {
            std::cerr << "Error: Cannot open output file '" << outputFile << "'\n";
            return 1;
        }
        sol.print(outFile);
    } else {
        sol.print(std::cout);
    }

    return 0;
}
