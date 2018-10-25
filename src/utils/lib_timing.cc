#include <iostream>
#include <pthread.h>

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
pthread_mutex_t g_timer_map_mutex;
bool g_timer_map_mutex_inited = false;

void start_timer(const std::string& timer_key) {
    // adding to timer pool needs to be thread safe
    if (!g_timer_map_mutex_inited) {
        g_timer_map_mutex_inited = true;
        pthread_mutex_init(&g_timer_map_mutex, NULL);
    }
    pthread_mutex_lock(&g_timer_map_mutex);
    g_timer_map[timer_key] = std::make_shared<Timer>();
    // start timer
    g_timer_map[timer_key]->startTimer();
    pthread_mutex_unlock(&g_timer_map_mutex);
}

void end_timer(const std::string& timer_key, std::ostream& os, uint32_t num_refs) {
    try {
        pthread_mutex_lock(&g_timer_map_mutex);
        const Timer::Handle& timer = g_timer_map.at(timer_key);
        timer->endTimer();
        std::string out_str = "timer <" + timer_key + "> elapsed:" +
            " total(s)=" + std::to_string(timer->getElapsedTime());
        if (num_refs > 0) {
            out_str += " per-ref(ns)=" + std::to_string(timer->getElapsedTime()/num_refs);
        }
        out_str += "\n";
        // remove it
        g_timer_map.erase(timer_key);
        pthread_mutex_unlock(&g_timer_map_mutex);
        os << out_str;
    } catch (const std::out_of_range& oor) {
        std::cerr << "Out of range error: " << oor.what() << " on " << timer_key << std::endl;
    } catch (...) {
        std::cerr << "Timer error ..." << std::endl;
    }
}

}
