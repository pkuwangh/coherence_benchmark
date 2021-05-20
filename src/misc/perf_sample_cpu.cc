#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <errno.h>
#include <linux/perf_event.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <unistd.h>

static inline uint64_t timevaldiff_us(
  const struct timeval& t1, const struct timeval& t2) {
    return (t2.tv_sec * 1000000 + t2.tv_usec) - (t1.tv_sec * 1000000 + t1.tv_usec);
}

class Channel {
 public:
  using ht_page_cnt = std::unordered_map<uint64_t, uint32_t>;

  Channel() {
    fd_ = -1;
  }
  ~Channel() {
    unbind();
  }

  int bind(uint32_t core_idx, uint64_t period);
  void unbind();
  int enable();
  int disable();
  void readPebsData(ht_page_cnt& page_count);

 private:
  int fd_;
  uint64_t sample_id_;
  void* buffer_;
};

struct perf_sample
{
    struct perf_event_header header;
    //uint64_t sample_id;     // PERF_SAMPLE_IDENTIFIER
    uint32_t pid, tid;      // PERF_SAMPLE_TID
    uint64_t address;       // PERF_SAMPLE_ADDR
    //uint32_t cpu, res;      // PERF_SAMPLE_CPU
};

#define PEBS_EVENT_ID           0x20D1
#define PAGE_SIZE               4096
#define RING_BUFFER_PAGES       64
#define BUFFER_SIZE             (RING_BUFFER_PAGES * PAGE_SIZE)
#define BUFFER_SIZE_RAW         (BUFFER_SIZE + PAGE_SIZE)
#define READ_MEMORY_BARRIER()   __builtin_ia32_lfence()

// wrapper of perf_event_open() syscall
static int perf_event_open(struct perf_event_attr *attr,
    pid_t pid, int cpu, int group_fd, unsigned long flags)
{
  return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
}

int Channel::bind(uint32_t core_idx, uint64_t period) {
  if (fd_ >= 0) {
    fprintf(stderr, "Channel already bound\n");
    return -1;
  }
  struct perf_event_attr attr;
  memset(&attr, 0, sizeof(struct perf_event_attr));
  attr.type = PERF_TYPE_RAW;
  attr.size = sizeof(struct perf_event_attr);
  attr.config = (uint64_t)PEBS_EVENT_ID;
  attr.sample_period = period;
  // sample id, pid, tid, address and cpu
  attr.sample_type = PERF_SAMPLE_ADDR |           // VA
                     //PERF_SAMPLE_IDENTIFIER |     // sample id
                     //PERF_SAMPLE_CPU |            // cpu id
                     PERF_SAMPLE_TID |            // pid, tid
                     (uint64_t)0x0;
  attr.disabled = 1;
  attr.exclude_kernel = 1;
  attr.precise_ip = 2;        // TODO: does it matter for address
  attr.wakeup_events = 1;
  // open perf event
  fd_ = perf_event_open(&attr, -1, core_idx, -1, 0);
  if (fd_ < 0) {
    fprintf(stderr, "perf_event_open failed, errno=%d\n", errno);
    return -errno;
  }
  // create ring buffer
  buffer_ = mmap(NULL, BUFFER_SIZE_RAW, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
  if (buffer_ == MAP_FAILED) {
    fprintf(stderr, "mmap failed to create ring buffer, errno=%d\n", errno);
    return -errno;
  }
  // get unique sample id
  if (ioctl(fd_, PERF_EVENT_IOC_ID, &sample_id_) < 0) {
    fprintf(stderr, "ioctl failed to get event ID, errno=%d\n", errno);
    return -errno;
  }
  fprintf(stdout, "core %u has unique sample id %lu\n", core_idx, sample_id_);
  return 0;
}

void Channel::unbind() {
  if (fd_ < 0) return;
  int ret = munmap(buffer_, BUFFER_SIZE_RAW);
  assert(ret == 0);
  ret = close(fd_);
  assert(ret == 0);
  fd_ = -1;
}

int Channel::enable() {
  assert(fd_ >= 0);
  int ret = ioctl(fd_, PERF_EVENT_IOC_ENABLE, 0);
  if (ret < 0) {
    fprintf(stderr, "ioctl failed to enable perf_event, errno=%d\n", errno);
  }
  fprintf(stderr, "enabled perf_event for id=%lu\n", sample_id_);
  return ret;
}

int Channel::disable() {
  assert(fd_ >= 0);
  int ret = ioctl(fd_, PERF_EVENT_IOC_DISABLE, 0);
  if (ret < 0) {
    fprintf(stderr, "ioctl failed to enable perf_event, errno=%d\n", errno);
  }
  fprintf(stderr, "disabled perf_event for id=%lu\n", sample_id_);
  return ret;
}

void Channel::readPebsData(ht_page_cnt& page_count) {
  struct timeval t0, t1, t2;
  gettimeofday(&t0, NULL);
  uint32_t num_samples = 0;
  uint32_t num_others = 0;
  uint64_t head = 0;
  uint64_t tail = 0;
  uint64_t size = 0;
  const uint64_t target_size = BUFFER_SIZE / 2;
  assert(fd_ >= 0);
  uint64_t sleep_duration = 200000;
  enable();
  while (true) {
      usleep(sleep_duration);
      gettimeofday(&t1, NULL);
      // scan through buffers
      {
          // first page is a metadata header page
          auto* metadata = (struct perf_event_mmap_page*)buffer_;
          tail = metadata->data_tail;
          head = metadata->data_head;
          size = head - tail;
          assert(tail <= head);
          READ_MEMORY_BARRIER();
          while (tail < head - 128) {
              // head & tail never wrap; skip header page
              uint64_t position = tail % BUFFER_SIZE + PAGE_SIZE;
              auto* entry = (struct perf_sample*)((char*)buffer_ + position);
              tail += entry->header.size;
              // read the record
              // PERF_RECORD_SAMPLE: 9
              // PERF_RECORD_THROTTLE: 5
              // PERF_RECORD_LOST: 2
              if (entry->header.type == PERF_RECORD_SAMPLE
                      //&& entry->sample_id == sample_id_
                 ) {
                  //fprintf(stdout, "head=%lu tail=%lu type=%lu id=%lu "
                  //        "cpu=%u pid=%u/%u addr=%#lx\n",
                  //        head, tail, entry->header.type, entry->sample_id,
                  //        entry->cpu, entry->pid, entry->tid, entry->address);
                  page_count[entry->address >> 12] += 1;
                  ++num_samples;
              } else {
                  //fprintf(stdout, "head=%lu tail=%lu type=%lu id=%lu\n",
                  //        head, tail, entry->header.type, entry->sample_id);
                  ++num_others;
              }
          }
          // update data_tail to notify kernel write new data
          assert(tail <= head);
          metadata->data_tail = tail;
      }
      gettimeofday(&t2, NULL);
      uint64_t process_duration = timevaldiff_us(t1, t2);
      uint64_t new_sleep_duration = (sleep_duration + process_duration) * target_size / size;
      if (new_sleep_duration > process_duration) {
          new_sleep_duration -= process_duration;
      }
      fprintf(stdout, "size=%lu target=%lu sleep=%.3f process=%.3f new_sleep=%.6f\n",
              size, target_size, sleep_duration/1e6, process_duration/1e6, new_sleep_duration/1e6);
      sleep_duration = new_sleep_duration;
      if (timevaldiff_us(t0, t2) > 10 * 1000000) {
          break;
      }
  }
  gettimeofday(&t2, NULL);
  fprintf(stdout, "reading takes %.3f, num_samples=%u num_others=%u\n",
      timevaldiff_us(t0, t2)/1e6, num_samples, num_others);
}


void processStats(Channel::ht_page_cnt& page_count) {
  uint32_t num_pages = 0;
  uint32_t num_accesses = 0;
  for (auto& iter : page_count) {
    num_pages += 1;
    num_accesses += iter.second;
  }
  fprintf(stdout, "num_pages=%-8u num_accesses=%-8u\n", num_pages, num_accesses);
  page_count.clear();
}


int main(int argc, char* argv[])
{
  if (argc < 3) {
    fprintf(stderr, "usage %s [period] [core id]\n", argv[0]);
    return 1;
  }
  uint32_t period = std::stoul(argv[1]);
  uint32_t core_idx = std::stoul(argv[2]);
  Channel::ht_page_cnt page_count;
  Channel pebs_channel;
  int ret = pebs_channel.bind(core_idx, period);
  if (ret != 0) return ret;
  pebs_channel.readPebsData(page_count);
  processStats(page_count);
  return 0;
}
