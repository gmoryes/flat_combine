#include "Logger.h"

std::mutex log_mutex;

void my_log(std::stringstream &ss) {
#if (LOG != 0)
    std::lock_guard<std::mutex> lock(log_mutex);
    std::string str = ss.str();
    str += "\n";
    std::cout << str;
#endif
}
