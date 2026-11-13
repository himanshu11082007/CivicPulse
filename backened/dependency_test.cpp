#include "third_party/httplib/httplib.h"
#include "third_party/nlohmann/json.hpp"
#include <iostream>

int main() {
    nlohmann::json data;
    data["project"] = "CivicPulse";
    data["status"] = "ready";

    std::cout << data.dump() << std::endl;
    return 0;
}
