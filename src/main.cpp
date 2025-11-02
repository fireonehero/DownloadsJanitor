#include <iostream>

#include <nlohmann/json.hpp>

int main()
{
    nlohmann::json status = {
        {"application", "DownloadsJanitor"},
        {"status", "ready"}
    };

    std::cout << status.dump() << std::endl;
    return 0;
}
