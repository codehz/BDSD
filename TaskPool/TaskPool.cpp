#include "TaskPool.h"

TaskPool::TaskPool() : thread{std::bind_front(&TaskPool::Worker, this)} {}

TaskPool::~TaskPool() {
  {
    std::lock_guard lock{mtx};
    stop = true;
  }
  cv.notify_all();
  if (thread.joinable()) thread.join();
}

void TaskPool::AddTask(std::function<void()> &&fn) {
  std::unique_lock lock{mtx};
  auto may_wait = tasks.empty();
  tasks.emplace(std::move(fn));
  lock.unlock();
  cv.notify_all();
}

void TaskPool::Worker() {
  while (true) {
    std::unique_lock lock{mtx};
    if (!tasks.empty()) {
      auto task = std::move(tasks.front());
      tasks.pop();
      lock.unlock();
      task();
    } else if (stop)
      return;
    else
      cv.wait(lock);
  }
}