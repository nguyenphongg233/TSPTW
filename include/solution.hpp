#ifndef SOLUTION_HPP
#define SOLUTION_HPP

#include <string>
#include <vector> 
#include <iostream> 
#include <cstdint>

struct Solution{
    std :: vector<int> route;
    std :: vector<int64_t> finishTime;
    int64_t travelCost = 0,completionTime = 0;
    bool feasible = false;

    Solution() = default;
    Solution(int n) : route(n), finishTime(n), travelCost(0), completionTime(0),feasible(false){}

    void print(std::ostream& out = std::cout) const {
        out << route.size() << "\n";
        for (size_t i = 0; i < route.size(); i++) {
            out << route[i] << (i + 1 < route.size() ? " " : "");
        }
        out << "\n";
    }

    void debug(){
        std :: cerr << "-----\ntravelCost - completionTime - feasible\n";
        std :: cerr << travelCost << " " << completionTime << " " << feasible << "\n";
        std :: cerr << route.size() << "\n";
        for(int i = 0;i < (int)route.size();i++){
            std :: cerr << route[i] << " " << finishTime[i] << "\n";
        }
    }

};

#endif