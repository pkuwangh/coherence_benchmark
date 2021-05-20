#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>
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
  using ht_page_cnt = std::unordered_map<uint32_t, std::unordered_map<uint64_t, uint32_t>>;

  Channel(uint32_t num_cores, uint64_t period) :
      fd_(num_cores, -1),
      id_(num_cores, 0),
      buffer_(num_cores, nullptr)
  {
    bind(num_cores, period);
  }
  ~Channel() {
    unbind();
  }

  int bind(uint32_t num_cores, uint64_t period);
  void unbind();
  int enable(uint32_t core_idx);
  int disable(uint32_t core_idx);
  void readPebsData(uint32_t core_start, uint32_t core_end, ht_page_cnt& page_count);

 private:
  std::vector<int> fd_;
  std::vector<uint64_t> id_;
  std::vector<void*> buffer_;
};

struct perf_sample
{
    struct perf_event_header header;
    //uint64_t id;          // PERF_SAMPLE_IDENTIFIER
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

int Channel::bind(uint32_t num_cores, uint64_t period) {
  for (uint32_t i = 0; i < num_cores; ++i) {
    if (fd_[i] >= 0) {
      fprintf(stderr, "Channel-%u already bound\n");
      return -1;
    }
  }
  for (uint32_t core_idx = 0; core_idx < num_cores; ++core_idx) {
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
    fd_[core_idx] = perf_event_open(&attr, -1, core_idx, -1, 0);
    if (fd_[core_idx] < 0) {
      fprintf(stderr, "perf_event_open failed, errno=%d\n", errno);
      return -errno;
    }
    // create ring buffer
    buffer_[core_idx] = mmap(
      NULL, BUFFER_SIZE_RAW,
      PROT_READ | PROT_WRITE, MAP_SHARED,
      fd_[core_idx], 0);
    if (buffer_[core_idx] == MAP_FAILED) {
      fprintf(stderr, "mmap failed to create ring buffer, errno=%d\n", errno);
      return -errno;
    }
    // get unique sample id
    if (ioctl(fd_[core_idx], PERF_EVENT_IOC_ID, &id_[core_idx]) < 0) {
      fprintf(stderr, "ioctl failed to get event ID, errno=%d\n", errno);
      return -errno;
    }
  }
  return 0;
}

void Channel::unbind() {
  for (uint32_t i = 0; i < fd_.size(); ++i) {
    if (fd_[i] < 0) continue;
    int ret = munmap(buffer_[i], BUFFER_SIZE_RAW);
    assert(ret == 0);
    ret = close(fd_[i]);
    assert(ret == 0);
    fd_[i] = -1;
  }
}

int Channel::enable(uint32_t core_idx) {
  assert(fd_[core_idx] >= 0);
  int ret = ioctl(fd_[core_idx], PERF_EVENT_IOC_ENABLE, 0);
  if (ret < 0) {
    fprintf(stderr, "ioctl failed to enable perf_event on core=%u errno=%d\n",
            core_idx, errno);
  }
  fprintf(stderr, "enabled perf_event on core=%u id=%lu\n", core_idx, id_[core_idx]);
  return ret;
}

int Channel::disable(uint32_t core_idx) {
  assert(fd_[core_idx] >= 0);
  int ret = ioctl(fd_[core_idx], PERF_EVENT_IOC_DISABLE, 0);
  if (ret < 0) {
    fprintf(stderr, "ioctl failed to disable perf_event on core=%u errno=%d\n",
            core_idx, errno);
  }
  fprintf(stderr, "disabled perf_event on core=%u id=%lu\n", core_idx, id_[core_idx]);
  return ret;
}

void processStats(Channel::ht_page_cnt& page_count) {
  for (auto& x : page_count) {
    fprintf(stdout, "process - %u\n", x.first);
    uint32_t num_pages = 0;
    uint32_t num_accesses = 0;
    for (auto& y : x.second) {
      num_pages += 1;
      num_accesses += y.second;
    }
    fprintf(stdout, "\tnum_pages=%-8u num_accesses=%-8u\n", num_pages, num_accesses);
  }
}

void Channel::readPebsData(uint32_t core_start, uint32_t core_end, ht_page_cnt& page_count) {
  struct timeval t0, t1, t2;
  gettimeofday(&t0, NULL);
  uint32_t num_samples = 0;
  uint32_t num_others = 0;
  const uint64_t target_size = BUFFER_SIZE / 2;
  uint64_t sleep_duration_us = 200000;
  while (true) {
      usleep(sleep_duration_us);
      gettimeofday(&t1, NULL);
      uint64_t max_size = 0;
      for (uint32_t i = core_start; i < core_end; ++i) {
          // first page is a metadata header page
          auto* metadata = (struct perf_event_mmap_page*)buffer_[i];
          uint64_t tail = metadata->data_tail;
          uint64_t head = metadata->data_head;
          uint64_t size = head - tail;
          if (size > max_size) {
            max_size = size;
          }
          assert(tail <= head);
          READ_MEMORY_BARRIER();
          while (tail +128 < head) {
              // head & tail never wrap; skip header page
              uint64_t position = tail % BUFFER_SIZE + PAGE_SIZE;
              auto* entry = (struct perf_sample*)((char*)buffer_[i] + position);
              tail += entry->header.size;
              // read the record
              // PERF_RECORD_SAMPLE: 9
              // PERF_RECORD_THROTTLE: 5
              // PERF_RECORD_LOST: 2
              if (entry->header.type == PERF_RECORD_SAMPLE
                    //&& entry->id == id_
                 ) {
                  //fprintf(stdout, "head=%lu tail=%lu type=%lu id=%lu "
                  //        "cpu=%u pid=%u/%u addr=%#lx\n",
                  //        head, tail, entry->header.type, id_[i],
                  //        i, entry->pid, entry->tid, entry->address);
                  if (page_count.count(entry->pid) == 0) {
                      page_count[entry->pid] = std::unordered_map<uint64_t, uint32_t>();
                  }
                  page_count[entry->pid][entry->address >> 12] += 1;
                  ++num_samples;
              } else {
                  //fprintf(stdout, "head=%lu tail=%lu type=%lu id=%lu\n",
                  //        head, tail, entry->header.type, id_[i]);
                  ++num_others;
              }
          }
          // update data_tail to notify kernel write new data
          assert(tail <= head);
          metadata->data_tail = tail;
      }
      gettimeofday(&t2, NULL);
      uint64_t process_duration_us = timevaldiff_us(t1, t2);
      uint64_t total_duration_us = sleep_duration_us + process_duration_us;
      uint64_t new_sleep_duration_us = total_duration_us;
      if (max_size > 0) {
          new_sleep_duration_us *= (1.0 * target_size / max_size);
      }
      if (new_sleep_duration_us > process_duration_us) {
          new_sleep_duration_us -= process_duration_us;
      }
      if (new_sleep_duration_us > 1000000) {
          new_sleep_duration_us = 1000000;
      }
      fprintf(stdout, "size=%lu target=%lu sleep=%.3f process=%.3f new_sleep=%.6f\n",
              max_size, target_size,
              sleep_duration_us/1e6, process_duration_us/1e6, new_sleep_duration_us/1e6);
      sleep_duration_us = new_sleep_duration_us;
      if (timevaldiff_us(t0, t2) > 10 * 1000000) {
          break;
      }
  }
  gettimeofday(&t2, NULL);
  fprintf(stdout, "reading takes %.3f, num_samples=%u num_others=%u\n",
      timevaldiff_us(t0, t2)/1e6, num_samples, num_others);
}


int main(int argc, char* argv[])
{
  if (argc < 3) {
    fprintf(stderr, "usage %s [period] [# of cores]\n", argv[0]);
    return 1;
  }
  uint32_t period = std::stoul(argv[1]);
  uint32_t num_cores = std::stoul(argv[2]);
  Channel::ht_page_cnt page_count;
  Channel pebs_channel(num_cores, period);
  uint32_t num_cores_in_group = 4;
  assert(num_cores % num_cores_in_group == 0);
  for (uint32_t i = 0; i < 4; ++i) {
    uint32_t c0 = (i * num_cores_in_group) % num_cores;
    uint32_t c1 = c0 + num_cores_in_group;
    for (uint32_t j = c0; j < c1; ++j) {
      pebs_channel.enable(j);
    }
    pebs_channel.readPebsData(c0, c1, page_count);
    for (uint32_t j = c0; j < c1; ++j) {
      pebs_channel.disable(j);
    }
  }
  processStats(page_count);
  return 0;
}
