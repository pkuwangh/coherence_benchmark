#include <iostream>
#include <mutex>
#include <unordered_map>

#include "utils/lib_timing.hh"

namespace utils {

Timer::Timer() :
    elapsed_time_ (0)
{
}

void Timer::startTimer() {
    time_point_begin_ = std::chrono::steady_clock::now();
}

void Timer::endTimer() {
    time_point_end_ = std::chrono::steady_clock::now();
    elapsed_time_ += std::chrono::duration_cast<std::chrono::duration<float>>(time_point_end_ - time_point_begin_).count();
}


std::unordered_map<std::string, Timer::Handle> g_timer_map;
std::mutex g_timer_map_mu;

void start_timer(const std::string& timer_key) {
    std::lock_guard<std::mutex> lock(g_timer_map_mu);
    // adding to timer pool needs to be thread safe
    g_timer_map[timer_key] = std::make_shared<Timer>();
    // start timer
    g_timer_map[timer_key]->startTimer();
}

float end_timer(const std::string& timer_key, std::ostream& os) {
    float elapsed_time = 0;
    {
        try {
            std::lock_guard<std::mutex> lock(g_timer_map_mu);
            const Timer::Handle& timer = g_timer_map.at(timer_key);
            timer->endTimer();
            elapsed_time = timer->getElapsedTime();
            // remove it
            g_timer_map.erase(timer_key);
        } catch (const std::out_of_range& oor) {
            std::cerr << "Out of range error: " << oor.what() << " on " << timer_key << std::endl;
        } catch (...) {
            std::cerr << "Timer error ..." << std::endl;
        }
    }
    std::string out_str = "timer <" + timer_key + "> elapsed:" +
        " total(s)=" + std::to_string(elapsed_time) + "\n";
    os << out_str;
    return elapsed_time;
}

void end_timer(const std::string& timer_key, std::ostream& os, uint64_t num_refs, float core_freq_ghz) {
    if (num_refs > 0) {
        const double elapsed_time = end_timer(timer_key, os);
        std::string out_str = "per-ref(ns)=" + std::to_string(1000000000 * elapsed_time / num_refs);
        out_str += ", per-ref(cycle)=" + std::to_string(1000000000 * elapsed_time / num_refs * core_freq_ghz);
        out_str += "\n\n";
        os << out_str;
    }
}

void end_timer(const std::string& timer_key, std::ostream& os, uint64_t size, uint64_t num_iters, float core_freq_ghz) {
    if (num_iters > 0) {
        const double elapsed_time = end_timer(timer_key, os);
        const double bw_bps = size * num_iters / elapsed_time;
        std::string out_str = "bw(MBpS)=" + std::to_string(bw_bps / 1024 / 1024);
        out_str += ", bw(BytesPerNs)=" + std::to_string(bw_bps / 1000000000);
        out_str += ", bw(BytesPerCycle)=" + std::to_string(bw_bps / 1000000000 / core_freq_ghz);
        out_str += "\n\n";
        os << out_str;
    }
}

}
