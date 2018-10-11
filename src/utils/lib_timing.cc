#include <iostream>
#include <pthread.h>

#include "utils/lib_timing.hh"

namespace utils {

void Timer::startTimer() {
    time_point_begin_ = std::chrono::steady_clock::now();
}

void Timer::endTimer(std::ostream& os) {
    time_point_end_ = std::chrono::steady_clock::now();
    os << std::chrono::duration_cast<std::chrono::duration<float>>(time_point_end_ - time_point_begin_).count()
        << '\n';
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
    pthread_mutex_unlock(&g_timer_map_mutex);
    // start timer
    g_timer_map[timer_key]->startTimer();
}

void end_timer(const std::string& timer_key, std::ostream& os) {
    try {
        os << "timer <" << timer_key << ">: elapsed time ";
        const Timer::Handle& timer = g_timer_map.at(timer_key);
        timer->endTimer(os);
        // remove it
        g_timer_map.erase(timer_key);
        if (g_timer_map.size() == 0) {
            pthread_mutex_destroy(&g_timer_map_mutex);
        }
    } catch (const std::out_of_range& oor) {
        os << "Out of range error: " << oor.what() << std::endl;
    } catch (...) {
        os << "Timer error ..." << std::endl;
    }
}

}
