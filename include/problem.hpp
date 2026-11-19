#ifndef PROBLEM_HPP
#define PROBLEM_HPP

#include <string>
#include <vector> 
#include <istream> 

struct Customer{
    int e,l,s;
    // earliest, latest, service
    explicit Customer(int e,int l,int s) : e(e), l(l), s(s){}
    Customer() : Customer(0,0,0){}
};

struct Problem{
    int n = 0;
    std :: vector<Customer> customers;
    std :: vector<int> travel;

    Problem () = default;
    explicit Problem(int n) : n(n) {
        customers.resize(n + 1);
        travel.resize(static_cast<size_t>(n + 1) * (n + 1));
    }

    int getTravel(int u,int v) const noexcept{
        return travel[static_cast<size_t>(u) * (n + 1) + v];
    }

    bool loadFromStream(std :: istream &in);
    bool loadFromFile(const std :: string& filename);

};

#endif