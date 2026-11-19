#ifndef UTILS_HPP
#define UTILS_HPP

#include "problem.hpp"
#include "solution.hpp"
#include <chrono>
#include <cstdint>
#include <string>


struct TestGenConfig {
    uint32_t seed     = 23;
    int maxCoord      = 1000; 
    int minService    = 5;    
    int maxService    = 30;   
    int nearestK      = 5;    
    int windowSlack   = 300;  
    int waitChancePct = 20; // 20%   
    int maxWaitTime   = 20;   
};

struct SolverConfig {
    uint32_t seed          = 42;
    double timeLimitSec    = 20.0;  
    int candidateListSize  = 20;     
    double initialTemp     = 1000.0; 
    double coolingRate     = 0.9995; 
    double minDestroyRatio = 0.02;   
    double maxDestroyRatio = 0.15;   
};

struct Timer {
    std::chrono::steady_clock::time_point startTime;

    Timer() : startTime(std::chrono::steady_clock::now()) {}
    void reset() noexcept {
        startTime = std::chrono::steady_clock::now();
    }

    double elapsedSeconds() const noexcept {
        std::chrono::duration<double> diff = std::chrono::steady_clock::now() - startTime;
        return diff.count();
    }

    bool isTimeUp(double limitSeconds) const noexcept {
        return elapsedSeconds() >= limitSeconds;
    }
};

Problem generateTestCase(int n,
                         const TestGenConfig& cfg = TestGenConfig{},
                         Solution* outPlantedSol = nullptr);

bool saveProblemToFile(const Problem& problem, const std::string& filename);

// Sinh toàn bộ bộ dữ liệu chuẩn (30 test nhỏ + 45 test lớn) vào thư mục outputDir
bool generateDataset(const std::string& outputDir = "data/input");

#endif
