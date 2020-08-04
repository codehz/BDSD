#pragma once
#include <thread>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <optional>
#include <functional>

class TaskPool {
  bool stop = false;
  std::mutex mtx;
  std::thread thread;
  std::condition_variable cv;
  std::queue<std::function<void()>> tasks;

  void Worker();

public:
  TaskPool();
  ~TaskPool();
  void AddTask(std::function<void()> &&);
};