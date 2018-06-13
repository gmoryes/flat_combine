#ifndef FLAT_COMBINE_LOGGER_H
#define FLAT_COMBINE_LOGGER_H

#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

extern std::mutex log_mutex;

void my_log(std::stringstream &ss);

#endif //FLAT_COMBINE_LOGGER_H
