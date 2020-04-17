//
// Created by bas on 11.01.17.
//

#ifndef BROKER_TIMEDCACHE_H
#define BROKER_TIMEDCACHE_H

#include <Poco/RWLock.h>
#include <Poco/Stopwatch.h>
#include <Poco/Timer.h>
#include <unordered_map>

namespace upmq {
namespace broker {

class Broker;

/// \class TimedCache
/// \brief Used for lazy closeing of connection by time or disconnect frame
class TimedCache {
 public:
  /// \brief CacheMap - map of connections ids and live time in seconds
  using CacheMap = std::unordered_map<std::string, int>;
  enum class Tm : int { default_time = 30 };

 private:
  Broker &_broker;
  CacheMap _cache;
  Poco::Stopwatch _sw;
  mutable Poco::RWLock _cacheLock;

 public:
  explicit TimedCache(Broker &broker);
  void onTimer(Poco::Timer &timer);
  void add(const std::string &connectionID, int tm = static_cast<int>(Tm::default_time));
  void erase(const std::string &connectionID);
  void disable(const std::string &connectionID);
  void remove(const std::string &connectionID);
};
}  // namespace broker
}  // namespace upmq

#endif  // BROKER_TIMEDCACHE_H
