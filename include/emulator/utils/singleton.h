#pragma once

template <typename T> class Singleton {
public:
  Singleton(const Singleton &) = delete;
  Singleton &operator=(const Singleton &) = delete;

  static T &getInstance();

protected:
  Singleton() = default;
  ~Singleton() = default;
};
