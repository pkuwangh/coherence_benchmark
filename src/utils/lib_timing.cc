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

void start_timer(const std::string& timer_key) {
    g_timer_map[timer_key] = std::make_shared<Timer>();
    g_timer_map[timer_key]->startTimer();
}

void end_timer(const std::string& timer_key, std::ostream& os) {
    try {
        os << "timer <" << timer_key << ">: elapsed time ";
        (g_timer_map.at(timer_key))->endTimer(os);
    } catch (const std::out_of_range& oor) {
        os << "Out of range error: " << oor.what() << std::endl;
    } catch (...) {
        os << "Timer error ..." << std::endl;
    }
}

}
