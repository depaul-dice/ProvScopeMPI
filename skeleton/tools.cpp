#include "tools.h"


bool Tools::isContained(std::map<long, long> const& m, long key) {
    if(m.find(key) == m.end()) {
        return false;
    } else {
        return true;
    }
}

bool Tools::stob(std::string str) {
    if(str == "true" || str == "True") return true;
    else if(str == "false" || str == "False") return false;
    std::cerr << "str: " << str << "\nin order to convert string to boolean it has to be either \"True\" or \"False\"\nAborting...\n";
    exit(1);
}

double Tools::average(const std::vector<double>& vec) {
    if (vec.empty()) {
        return 0.0;
    }
    double sum = std::accumulate(vec.begin(), vec.end(), 0.0);
    return sum / vec.size();   
}
