#include "evaluator.hpp"
#include "problem.hpp"
#include "solution.hpp"
#include "utils.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

// Hàm giải 1 bài toán (nơi bạn gắn Heuristic / Local Search / ALNS vào)
Solution solveInstance(const Problem& problem) {
    // TODO: Thay bằng hàm Heuristic / ALNS khi cài đặt ở bước tiếp theo
    Solution sol(problem.n);
    for (int i = 0; i < problem.n; i++) {
        sol.route[i] = i + 1;
    }
    evaluateSolution(problem, sol);
    return sol;
}

void benchmark(const std::string& mode = "all") {
    std::string inDir = "data/input/";
    std::string outDir = "data/output/alns/";
    std::string resDir = "results/";

    std::filesystem::create_directories(outDir);
    std::filesystem::create_directories(resDir);

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

    std::string csvPath = resDir + "benchmark_results.csv";
    std::ofstream csvOut(csvPath);
    csvOut << "instance,n,group,feasible,travel_cost,completion_time,runtime_sec\n";

    std::cout << std::left
              << std::setw(22) << "Instance"
              << std::setw(8)  << "N"
              << std::setw(10) << "Group"
              << std::setw(10) << "Feasible"
              << std::setw(14) << "TravelCost"
              << std::setw(16) << "CompletionTime"
              << std::setw(12) << "Time(s)" << "\n";
    std::cout << std::string(92, '-') << "\n";

    int totalCases = 0;
    int feasibleCases = 0;
    int64_t sumFeasibleCost = 0;
    double totalTime = 0.0;

    for (const auto& tc : cases) {
        std::string inPath = inDir + tc.name + ".inp";
        std::string outPath = outDir + tc.name + ".out";

        Problem problem;
        if (!problem.loadFromFile(inPath)) {
            std::cerr << "[Skip] Cannot load " << inPath << " (run --gen first)\n";
            continue;
        }

        Timer timer;
        Solution sol = solveInstance(problem);
        double elapsed = timer.elapsedSeconds();

        // Chấm lại bằng Evaluator đầy đủ (check cả hoán vị lẫn time window)
        evaluateSolution(problem, sol, true);

        // Ghi file .out
        std::ofstream outFile(outPath);
        if (outFile.is_open()) {
            sol.print(outFile);
        }

        totalCases++;
        totalTime += elapsed;
        if (sol.feasible) {
            feasibleCases++;
            sumFeasibleCost += sol.travelCost;
        }

        std::cout << std::left
                  << std::setw(22) << tc.name
                  << std::setw(8)  << problem.n
                  << std::setw(10) << tc.group
                  << std::setw(10) << (sol.feasible ? "YES" : "NO")
                  << std::setw(14) << (sol.feasible ? std::to_string(sol.travelCost) : "-")
                  << std::setw(16) << (sol.feasible ? std::to_string(sol.completionTime) : "-")
                  << std::fixed << std::setprecision(4) << std::setw(12) << elapsed << "\n";

        csvOut << tc.name << ","
               << problem.n << ","
               << tc.group << ","
               << (sol.feasible ? 1 : 0) << ","
               << sol.travelCost << ","
               << sol.completionTime << ","
               << std::fixed << std::setprecision(6) << elapsed << "\n";
    }

    std::cout << std::string(92, '-') << "\n";
    std::cout << "[Summary] Feasible: " << feasibleCases << "/" << totalCases
              << " | Avg Feasible Cost: " << (feasibleCases > 0 ? sumFeasibleCost / feasibleCases : 0)
              << " | Total Time: " << std::fixed << std::setprecision(4) << totalTime << "s\n";
    std::cout << "[Saved]   CSV results written to '" << csvPath << "'\n";
}

int main(int argc, char* argv[]) {
    if (argc >= 2 && std::string(argv[1]) == "--gen") {
        std::string outDir = (argc >= 3) ? argv[2] : "data/input";
        Timer timer;
        if (!generateDataset(outDir)) {
            std::cerr << "Error: Failed to generate dataset in " << outDir << "\n";
            return 1;
        }
        std::cout << "[Dataset] Done in " << timer.elapsedSeconds() << "s\n";
        return 0;
    }

    if (argc >= 2 && std::string(argv[1]) == "--bench") {
        std::string mode = (argc >= 3) ? argv[2] : "all";
        benchmark(mode);
        return 0;
    }

    if (argc < 2) {
        std::cerr << "Usage:\n"
                  << "  1. Generate dataset: " << argv[0] << " --gen [output_dir]\n"
                  << "  2. Run benchmark:    " << argv[0] << " --bench [all|small|big]\n"
                  << "  3. Solve instance:   " << argv[0] << " <input.inp> [output.out]\n";
        return 1;
    }

    std::string inputFile = argv[1];
    Timer timer;

    Problem problem;
    if (!problem.loadFromFile(inputFile)) {
        std::cerr << "Error: Cannot load input file '" << inputFile << "'\n";
        return 1;
    }
    std::cerr << "[Info] Loaded N = " << problem.n << " from '" << inputFile
              << "' in " << timer.elapsedSeconds() << "s\n";

    Solution sol = solveInstance(problem);
    sol.debug();

    if (argc >= 3) {
        std::ofstream outFile(argv[2]);
        if (!outFile.is_open()) {
            std::cerr << "Error: Cannot open output file '" << argv[2] << "'\n";
            return 1;
        }
        sol.print(outFile);
    } else {
        sol.print(std::cout);
    }

    return 0;
}
