#ifndef __LIB_TIMING_HH__
#define __LIB_TIMING_HH__

#include <chrono>
#include <memory>
#include <ostream>
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
float end_timer(const std::string& timer_key, std::ostream& os);
void end_timer(const std::string& timer_key, std::ostream& os, uint64_t num_refs, float core_freq_ghz);
void end_timer(const std::string& timer_key, std::ostream& os, uint64_t size, uint64_t num_iters, float core_freq_ghz);

}

#endif
