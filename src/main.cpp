#include "evaluator.hpp"
#include "problem.hpp"
#include "solution.hpp"
#include "utils.hpp"
#include <fstream>
#include <iostream>
#include <string>

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

    if (argc < 2) {
        std::cerr << "Usage:\n"
                  << "  1. Generate dataset: " << argv[0] << " --gen [output_dir]\n"
                  << "  2. Solve instance:   " << argv[0] << " <input.inp> [output.out]\n";
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

    // TODO: Gắn hàm tạo nghiệm (Heuristic / Local Search / ALNS) vào đây ở bước tiếp theo
    Solution sol(problem.n);
    for (int i = 0; i < problem.n; i++) {
        sol.route[i] = i + 1;
    }
    evaluateSolution(problem, sol);
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
