//
// Created by bas on 11.01.17.
//

#include "TimedCache.h"
#include "Broker.h"

namespace upmq {
namespace broker {

TimedCache::TimedCache(Broker &broker) : _broker(broker) { _sw.start(); }
void TimedCache::onTimer(Poco::Timer &timer) {
  Poco::ScopedWriteRWLock writeRWLock(_cacheLock);
  for (auto cn = _cache.begin(); cn != _cache.end();) {
    cn->second -= 1;
    if (cn->second <= 0) {
      if (cn->second != -88) {
        _broker.eraseConnection(cn->first);
      }
      _cache.erase(cn++);
    } else {
      ++cn;
    }
  }
}
void TimedCache::add(const std::string &connectionID, int tm) {
  {
    Poco::ScopedReadRWLock readRWLock(_cacheLock);
    auto it = _cache.find(connectionID);
    if (it != _cache.end()) {
      return;
    }
  }
  {
    Poco::ScopedWriteRWLock writeRWLock(_cacheLock);
    _cache.insert(std::make_pair(connectionID, tm));
  }
}
void TimedCache::erase(const std::string &connectionID) {
  remove(connectionID);
  _broker.eraseConnection(connectionID);
}
void TimedCache::disable(const std::string &connectionID) {
  Poco::ScopedReadRWLock readRWLock(_cacheLock);
  auto it = _cache.find(connectionID);
  if (it != _cache.end()) {
    it->second = -87;
  }
}
void TimedCache::remove(const std::string &connectionID) {
  Poco::ScopedWriteRWLock writeRWLock(_cacheLock);
  auto it = _cache.find(connectionID);
  if (it != _cache.end()) {
    _cache.erase(it);
  }
}
}  // namespace broker
}  // namespace upmq