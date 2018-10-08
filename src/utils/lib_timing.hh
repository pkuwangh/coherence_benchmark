#ifndef __LIB_TIMING_HH__
#define __LIB_TIMING_HH__

#include <chrono>
#include <memory>
#include <ostream>
#include <unordered_map>
#include <string>

namespace lib_timing {

class Timer {
  public:
    using Handle = std::shared_ptr<Timer>;

    Timer() { }
    ~Timer() = default;

    void startTimer();
    void endTimer(std::ostream& os);

  private:
    std::chrono::steady_clock::time_point time_point_begin_;
    std::chrono::steady_clock::time_point time_point_end_;
};


void start_timer(const std::string& timer_key);
void end_timer(const std::string& timer_key, std::ostream& os);

}

#endif
