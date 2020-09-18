#ifndef GAUGE_BENCHMARK_HPP
#define GAUGE_BENCHMARK_HPP
#include <chrono>
#include <unordered_map>

class Timer {
private:
    struct Average {
        long long observations_count = 0;
        double    average            = 0;
    };
    std::unordered_map<std::string, Average> averages;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point>
        pending;

public:
    Timer() = default;

    inline void start(std::string label) {
        pending[label] = std::chrono::steady_clock::now();
    }
    inline double stop(std::string label) {
        auto now        = std::chrono::steady_clock::now();
        auto start_time = pending.at(label);
        auto duration =
            std::chrono::duration<double>(now - start_time).count();
        averages[label].observations_count += 1;
        averages[label].average =
            (averages[label].average *
                 (averages[label].observations_count - 1) /
                 averages[label].observations_count +
             duration / averages[label].observations_count);
        return duration;
    }
    inline double get_average(std::string label) {
        return averages[label].average;
    }
};

#endif // GAUGE_BENCHMARK_HPP
