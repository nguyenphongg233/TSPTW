
#include "evaluator.hpp"

bool isValidPermutation(const Problem& problem, const std::vector<int>& route){
    int n = problem.n;
    if(route.size() != n)return false;
    std :: vector<bool> isExist (n);
    for(int i = 0;i < (int)route.size();i++){
        if(route[i] < 1 || route[i] > n)return false;
        if(isExist[route[i] - 1])return false;
        isExist[route[i] - 1] = true;
    }
    return true;
}

EvalResult evaluateRoute(const Problem& problem,const std::vector<int>& route,bool checkPermutation,std::vector<int64_t>* fillFinishTime){
    if(checkPermutation && !isValidPermutation(problem,route))return EvalResult(0,0,0,-1);
    if(route.size() != problem.n)return EvalResult(0,0,0,-1);

    if(fillFinishTime != nullptr) fillFinishTime -> resize(problem.n);

    int64_t currentTime = 0;
    int64_t cost = 0;
    int depot = 0;

    for(int i = 0;i < (int)route.size();i++){
        cost += problem.getTravel(depot,route[i]);
        currentTime += problem.getTravel(depot,route[i]);
        if(currentTime > problem.customers[route[i]].l)return EvalResult(0,0,0,i);
        if(currentTime < problem.customers[route[i]].e)
            currentTime = problem.customers[route[i]].e;
        currentTime += problem.customers[route[i]].s;
        if(currentTime > problem.customers[route[i]].l)return EvalResult(0,0,0,i);
        if(fillFinishTime != nullptr)(*fillFinishTime)[i] = currentTime;
        depot = route[i];
    }
    
    return EvalResult(true,cost,currentTime,-1);
}

bool evaluateSolution(const Problem& problem, Solution& sol, bool checkPermutation){
    EvalResult er = evaluateRoute(problem, sol.route, checkPermutation,&sol.finishTime);
    sol.travelCost = er.travelCost;
    sol.completionTime = er.completionTime;
    sol.feasible = er.feasible;
    return sol.feasible;
}
bool evaluateSuffix(const Problem& problem, Solution& sol, int startIdx) {
    if (startIdx <= 0) {
        return evaluateSolution(problem, sol, false);
    }
    if (static_cast<int>(sol.route.size()) != problem.n ||
        static_cast<int>(sol.finishTime.size()) != problem.n) {
        sol.feasible = false;
        return false;
    }

    int64_t currentTime = sol.finishTime[startIdx - 1];
    int prev = sol.route[startIdx - 1];

    for (int i = startIdx; i < problem.n; i++) {
        int u = sol.route[i];
        currentTime += problem.getTravel(prev, u);
        if (currentTime < problem.customers[u].e) {
            currentTime = problem.customers[u].e;
        }
        currentTime += problem.customers[u].s;
        if (currentTime > problem.customers[u].l) {
            sol.feasible = false;
            return false;
        }
        sol.finishTime[i] = currentTime;
        prev = u;
    }

    int64_t totalCost = 0;
    prev = 0;
    for (int i = 0; i < problem.n; i++) {
        totalCost += problem.getTravel(prev, sol.route[i]);
        prev = sol.route[i];
    }

    sol.travelCost = totalCost;
    sol.completionTime = currentTime;
    sol.feasible = true;
    return true;
}
