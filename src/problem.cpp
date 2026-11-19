#include "problem.hpp"
#include <fstream> 

bool Problem :: loadFromFile(const std :: string& filename){
    std :: ifstream file(filename);
    if(!file.is_open())return false;
    return loadFromStream(file);
}

bool Problem :: loadFromStream(std :: istream &in){
    int tn = 0;
    if(!(in >> tn) || tn < 0)return false;
    std :: vector<Customer> tc(tn + 1);
    std :: vector<int> tt(static_cast<size_t>(tn + 1) * (tn + 1));

    for(int i = 1;i <= tn;i++) in >> tc[i].e >> tc[i].l >> tc[i].s;
    for(int i = 0;i <= tn;i++){
        for(int j = 0;j <= tn;j++){
            if(!(in >> tt[static_cast<size_t> (i) * (tn + 1) + j]))return false;
        }
    }

    n = tn;
    customers = std :: move(tc);
    travel = std :: move(tt);

    return true;
}

