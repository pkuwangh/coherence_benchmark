#ifndef __LIB_TIMING_HH__
#define __LIB_TIMING_HH__

#include <chrono>
#include <memory>
#include <ostream>
#include <unordered_map>
#include <string>

namespace utils {

class Timer {
  public:
    using Handle = std::shared_ptr<Timer>;

    Timer();
    ~Timer() = default;

    void startTimer();
    void endTimer();

    const float& getElapsedTime() const { return elapsed_time_; }

  private:
    std::chrono::steady_clock::time_point time_point_begin_;
    std::chrono::steady_clock::time_point time_point_end_;
    float elapsed_time_;
};


void start_timer(const std::string& timer_key);
void end_timer(const std::string& timer_key, std::ostream& os);

}

#endif
