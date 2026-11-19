#include "evaluator.hpp"
#include "problem.hpp"
#include "solution.hpp"
#include "utils.hpp"
#include <cassert>
#include <iostream>
#include <sstream>

namespace {

Problem makeSampleProblemN2() {
    // N = 2
    // Customer 1: e = 10, l = 20, s = 5
    // Customer 2: e = 0,  l = 30, s = 4
    // Travel matrix (3x3):
    //   0 -> 1: 3, 0 -> 2: 8
    //   1 -> 0: 3, 1 -> 2: 6
    //   2 -> 0: 8, 2 -> 1: 6
    std::istringstream ss(
        "2\n"
        "10 20 5\n"
        "0 30 4\n"
        "0 3 8\n"
        "3 0 6\n"
        "8 6 0\n"
    );
    Problem p;
    bool ok = p.loadFromStream(ss);
    assert(ok);
    return p;
}

// 1. Test N = 1
void testSingleCustomer() {
    std::istringstream ss(
        "1\n"
        "5 15 4\n"
        "0 7\n"
        "99 0\n"
    );
    Problem p;
    assert(p.loadFromStream(ss));

    // 0 -> 1: arrival = 7 (>= e1=5), finish = 7 + 4 = 11 (<= l1=15)
    // Open route: does not return to 0 (99 is ignored)
    EvalResult res = evaluateRoute(p, {1});
    assert(res.feasible == true);
    assert(res.travelCost == 7);
    assert(res.completionTime == 11);
    assert(res.firstViolatedIndex == -1);
}

// 2. Test arriving earlier than e[i] (must wait) and open route (no return to depot)
void testWaitingAndOpenRoute() {
    Problem p = makeSampleProblemN2();

    // Route [1, 2]:
    // Step 0 (0 -> 1): travel = 3, arrival = 3 < e1(10) -> wait until 10, finish = 10 + 5 = 15 <= l1(20)
    // Step 1 (1 -> 2): travel = 6, arrival = 15 + 6 = 21 >= e2(0), finish = 21 + 4 = 25 <= l2(30)
    // Total travelCost = 3 + 6 = 9
    Solution sol(p.n);
    sol.route = {1, 2};
    assert(evaluateSolution(p, sol) == true);
    assert(sol.feasible == true);
    assert(sol.travelCost == 9);
    assert(sol.completionTime == 25);
    assert(sol.finishTime.size() == 2);
    assert(sol.finishTime[0] == 15);
    assert(sol.finishTime[1] == 25);
}

// 3. Test finishing exactly at l[i] (F[k] == l[s[k]] is still feasible)
void testExactDeadlineBoundary() {
    std::istringstream ss(
        "1\n"
        "5 15 5\n"
        "0 10\n"
        "10 0\n"
    );
    Problem p;
    assert(p.loadFromStream(ss));

    // arrival = 10, start = 10, finish = 10 + 5 = 15 == l1(15) -> feasible
    EvalResult res = evaluateRoute(p, {1});
    assert(res.feasible == true);
    assert(res.travelCost == 10);
    assert(res.completionTime == 15);
}

// 4. Test finishing after l[i] (F[k] > l[s[k]] is infeasible)
void testViolatedDeadline() {
    Problem p = makeSampleProblemN2();

    // Route [2, 1]:
    // Step 0 (0 -> 2): travel = 8, arrival = 8, finish = 8 + 4 = 12 <= l2(30)
    // Step 1 (2 -> 1): travel = 6, arrival = 12 + 6 = 18, finish = 18 + 5 = 23 > l1(20) -> violated at index 1!
    EvalResult res = evaluateRoute(p, {2, 1});
    assert(res.feasible == false);
    assert(res.firstViolatedIndex == 1);
}

// 5, 6, 7. Test invalid permutations (duplicate, missing, out-of-range)
void testInvalidPermutations() {
    Problem p = makeSampleProblemN2();

    // Duplicate customer
    assert(isValidPermutation(p, {1, 1}) == false);
    assert(evaluateRoute(p, {1, 1}).feasible == false);

    // Missing customer
    assert(isValidPermutation(p, {1}) == false);
    assert(evaluateRoute(p, {1}).feasible == false);

    // Extra customer
    assert(isValidPermutation(p, {1, 2, 1}) == false);

    // Out-of-range customer IDs (0 or N+1)
    assert(isValidPermutation(p, {0, 2}) == false);
    assert(isValidPermutation(p, {1, 3}) == false);
    assert(evaluateRoute(p, {1, 3}).feasible == false);
}

// 8. Test evaluateSuffix matches full evaluateSolution
void testEvaluateSuffix() {
    std::istringstream ss(
        "3\n"
        "0 50 2\n"
        "0 50 3\n"
        "0 50 4\n"
        "0 5 6 7\n"
        "5 0 4 8\n"
        "6 4 0 3\n"
        "7 8 3 0\n"
    );
    Problem p;
    assert(p.loadFromStream(ss));

    Solution sol(p.n);
    sol.route = {1, 2, 3};
    assert(evaluateSolution(p, sol) == true);

    // Swap positions 1 and 2 -> route becomes {1, 3, 2}, prefix at index 0 is unchanged
    sol.route = {1, 3, 2};
    Solution expected = sol;
    assert(evaluateSolution(p, expected) == true);

    assert(evaluateSuffix(p, sol, 1) == true);
    assert(sol.feasible == expected.feasible);
    assert(sol.travelCost == expected.travelCost);
    assert(sol.completionTime == expected.completionTime);
    assert(sol.finishTime == expected.finishTime);
}

// 9. Test generateTestCase with TestGenConfig produces 100% feasible planted solutions
void testGenerateTestCase() {
    TestGenConfig cfg;
    cfg.seed = 2026;
    cfg.windowSlack = 150;

    Solution planted(1);
    Problem p = generateTestCase(100, cfg, &planted);
    assert(p.n == 100);
    assert(planted.feasible == true);
    assert(isValidPermutation(p, planted.route) == true);
    assert(planted.travelCost > 0);
}

} // namespace

int main() {
    testSingleCustomer();
    testWaitingAndOpenRoute();
    testExactDeadlineBoundary();
    testViolatedDeadline();
    testInvalidPermutations();
    testEvaluateSuffix();
    testGenerateTestCase();

    std::cout << "[PASS] All 9 Evaluator & Generator tests passed successfully!\n";
    return 0;
}
