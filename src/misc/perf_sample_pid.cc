#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <errno.h>
#include <linux/perf_event.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#define ERROR(cleanup, ret, show_errstr, msgs...)                           \
({                                                                          \
    const char* _errstr = (show_errstr) ? strerror(errno) : "";             \
    (cleanup);                                                              \
    fprintf(stderr, "[<%s> @ %s: %d]: ", __FUNCTION__, __FILE__, __LINE__); \
    fprintf(stderr, ##msgs);                                                \
    fprintf(stderr, "%s\n", _errstr);                                       \
    return (ret);                                                           \
})

class Channel
{
 public:
  enum Type {
    CHANNEL_L3MISS_LOAD = 0x20D1
  };

  struct Sample {
    Type type;
    uint32_t cpu;       // on which cpu(core) this sample happens
    uint32_t pid;       // in which process(pid) and thread(tid) this sample happens
    uint32_t tid;
    uint64_t address;   // the virtual address in this process to be accessed
  };

  Channel() {
    m_fd = -1;
  }
  ~Channel() {
    unbind();
  }

  /* Initialize the Channel.
   *      pid:    the process to be sampled
   *      type:   type of instructions to be sampled
   * RETURN: 0 if OK, or a negative error code
   * NOTE: after calling bind(), the Channel remains disabled until setPeriod() is called.
   */
  int bind(pid_t pid, Type type);

  /* De-initialize the Channel.
   * NOTE: after calling unbind(), the Channel go back to uninitialized.
   */
  void unbind();

  /* Set the sample period.
   * Sample period means that a sample is triggered every how many instructions.
   * For example, if period is set to be 10000, then a sample happens every 10000 instructions.
   *      period: the period
   * RETURN: 0 if OK, or a negative error code
   * NOTE: a zero period disables this Channel. And there is a minimal threshold on it,
   * <period> is invalid if less than the threshold. The threshold varies between
   * different hardwares.
   */
  int setPeriod(unsigned long period);

  /* Read a sample from this Channel.
   *      sample: the buffer to receive the sample
   * RETURN: 0 if OK, -EAGAIN if not available, or a negative error code
   */
  int readSample(Sample* sample);

  /* Get the pid of target process.
   * RETURN: pid, or a meaningless value if uninitialized.
   */
  pid_t getPid() { return m_pid; }

  /* Get the type to sample.
   * RETURN: type, or a meaningless value if uninitialized.
   */
  Type getType() { return m_type; }

  /* Get the file descriptor from perf_event_open().
   * RETURN: the file descriptor, or -1 if uninitialized.
   * NOTE: Be careful with the fd, a wrong use of it will disturb the logic of this Channel.
   */
  int getPerfFd() { return m_fd; }

 private:
  pid_t m_pid;            // pid of target process
  Type m_type;            // type
  int m_fd;               // file descriptor from perf_event_open()
  uint64_t m_id;          // sample id of each record
  void* m_buffer;         // ring buffer and its header
  unsigned long m_period; // sample_period
};

#define WAKEUP_EVENTS           1
#define INIT_SAMPLE_PERIOD      100000
#define PAGE_SIZE               4096
#define RING_BUFFER_PAGES       4
#define MMAP_SIZE               ((1 + RING_BUFFER_PAGES) * PAGE_SIZE)
#define READ_MEMORY_BARRIER()   __builtin_ia32_lfence()

// wrapper of perf_event_open() syscall
static int perf_event_open(struct perf_event_attr *attr,
    pid_t pid, int cpu, int group_fd, unsigned long flags)
{
    return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
}

int Channel::bind(pid_t pid, Type type)
{
    if(m_fd >= 0)
        ERROR({}, -EINVAL, false, "this Channel has already bound");
    struct perf_event_attr attr;
    memset(&attr, 0, sizeof(struct perf_event_attr));
    attr.type = PERF_TYPE_RAW;
    attr.config = (uint64_t)type;
    attr.size = sizeof(struct perf_event_attr);
    attr.sample_period = INIT_SAMPLE_PERIOD;
    // sample id, pid, tid, address and cpu
    attr.sample_type = PERF_SAMPLE_IDENTIFIER | PERF_SAMPLE_TID | PERF_SAMPLE_ADDR |
        PERF_SAMPLE_CPU;
    attr.disabled = 1;
    attr.exclude_kernel = 1;
    attr.precise_ip = 3;
    attr.wakeup_events = WAKEUP_EVENTS;
    // open perf event
    int fd = perf_event_open(&attr, pid, -1, -1, 0);
    if(fd < 0) {
        int ret = -errno;
        ERROR({}, ret, true, "perf_event_open(&attr, %d, -1, -1, 0) failed: ", pid);
    }
    // create ring buffer
    void* buffer = mmap(NULL, MMAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(buffer == MAP_FAILED) {
        int ret = -errno;
        ERROR(close(fd), ret, true, "mmap(NULL, %u, PROT_READ | PROT_WRITE, MAP_SHARED, %d, 0)"
            " failed: ", MMAP_SIZE, fd);
    }
    // get id
    uint64_t id;
    int ret = ioctl(fd, PERF_EVENT_IOC_ID, &id);
    if(ret < 0) {
        int ret = -errno;
        ERROR({ munmap(buffer, MMAP_SIZE); close(fd); }, ret, true,
            "ioctl(%d, PERF_EVENT_IOC_ID, &id) failed: ", fd);
    }
    m_pid = pid;
    m_type = type;
    m_fd = fd;
    m_id = id;
    m_buffer = buffer;
    m_period = 0;
    return 0;
}

void Channel::unbind() {
    if(m_fd < 0)
        return;
    int ret = munmap(m_buffer, MMAP_SIZE);
    assert(ret == 0);
    ret = close(m_fd);
    assert(ret == 0);
    m_fd = -1;
}

int Channel::setPeriod(unsigned long period)
{
    if(m_fd < 0)
        ERROR({}, -EINVAL, false, "this Channel has not bound yet");
    if(period == m_period)
        return 0;
    int ret;
    // disable channel
    if(period == 0) {
        ret = ioctl(m_fd, PERF_EVENT_IOC_DISABLE, 0);
        if(ret < 0) {
            ret = -errno;
            ERROR({}, ret, true, "ioctl(%d, PERF_EVENT_IOC_DISABLE, 0) failed: ", m_fd);
        }
        m_period = 0;
        return 0;
    }
    // set new period
    ret = ioctl(m_fd, PERF_EVENT_IOC_PERIOD, &period);
    if(ret < 0) {
        ret = -errno;
        ERROR({}, ret, true, "ioctl(%d, PERF_EVENT_IOC_PERIOD, &(%lu)) failed: ",
            m_fd, period);
    }
    // if channel was disabled, enable it
    if(m_period == 0) {
        ret = ioctl(m_fd, PERF_EVENT_IOC_ENABLE, 0);
        if(ret < 0) {
            ret = -errno;
            ERROR({}, ret, true, "ioctl(%d, PERF_EVENT_IOC_ENABLE, 0) failed: ", m_fd);
        }
    }
    m_period = period;
    return 0;
}

// see man page for perf_event_open()
struct perf_sample
{
    struct perf_event_header header;
    uint64_t id;
    uint32_t pid, tid;
    uint64_t address;
    uint32_t cpu, ret;
};

int Channel::readSample(Sample* sample)
{
    if(m_fd < 0)
        ERROR({}, -EINVAL, false, "this Channel has not bound yet");
    // the header
    auto* meta = (struct perf_event_mmap_page*)m_buffer;
    uint64_t tail = meta->data_tail;
    uint64_t head = meta->data_head;
    READ_MEMORY_BARRIER();
    assert(tail <= head);
    if(tail == head)
        return -EAGAIN;
    bool available = false;
    while(tail < head)
    {
        // the data_head and data_tail never wrap, they are logical
        uint64_t position = tail % (PAGE_SIZE * RING_BUFFER_PAGES);
        auto* entry = (struct perf_sample*)((char*)m_buffer + PAGE_SIZE + position);
        tail += entry->header.size;
        // read the record
        if(entry->header.type == PERF_RECORD_SAMPLE && entry->id == m_id &&
            // this line is to filter the wrong pid caused by kernel bug
            entry->pid == m_pid)
        {
            sample->type = m_type;
            sample->cpu = entry->cpu;
            sample->pid = entry->pid;
            sample->tid = entry->tid;
            sample->address = entry->address;
            available = true;
            break;
        }
    }
    assert(tail <= head);
    // update data_tail to notify kernel write new data
    meta->data_tail = tail;
    return available ? 0 : -EAGAIN;
}


int main(int argc, char* argv[])
{
  unsigned long period;
  pid_t pid;
  if(argc != 3 ||
      sscanf(argv[1], "%lu", &period) != 1 ||
      sscanf(argv[2], "%d", &pid) != 1)
  {
    printf("USAGE: %s <period> <pid>\n", argv[0]);
    return 1;
  }
  Channel c;
  int ret = c.bind(pid, Channel::CHANNEL_L3MISS_LOAD);
  if(ret)
    return ret;
  ret = c.setPeriod(period);
  if(ret)
    return ret;
  while(true)
  {
    Channel::Sample sample;
    ret = c.readSample(&sample);
    if(ret == -EAGAIN)
    {
      usleep(10000);
      continue;
    }
    else if(ret < 0)
      return ret;
    else
      printf("type: %x, cpu: %u, pid: %u, tid: %u, address: %lx\n",
          sample.type, sample.cpu, sample.pid, sample.tid, sample.address);
  }
  return 0;
}
