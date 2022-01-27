//+*****************************************************************************
//                         Honeywell Proprietary
// This document and all information and expression contained herein are the
// property of Honeywell International Inc., are loaned in confidence, and
// may not, in whole or in part, be used, duplicated or disclosed for any
// purpose without prior written permission of Honeywell International Inc.
//               This document is an unpublished work.
//
// Copyright (C) 2020 Honeywell International Inc. All rights reserved.
// *****************************************************************************
// Contributors:
//      MiD   Michal Dobes
//
//+*****************************************************************************
/* 
 * File:   ExpirationMap.hpp
 * Author: Michal Dobes <michal.dobes2 at honeywell.com>
 *
 * Created on March 25, 2019, 11:53 AM
 */

#pragma once

#include <chrono>
#include <condition_variable>
#include <experimental/optional>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <thread>


namespace ExpirationMap {
  using Duration = std::chrono::duration<long, std::ratio<1,1000>>;
  using TimePoint = std::chrono::steady_clock::time_point;

  template<typename K, typename V>
  class ExpirationMap : public std::enable_shared_from_this<ExpirationMap<K, V>> {
  public:
    using ExpiredValues = std::map<K, V>;
    
    ExpirationMap() {}
    ExpirationMap(const ExpirationMap& other) = delete;
    ExpirationMap(ExpirationMap&& other) = delete;

    /**
     * Insert an item into the map
     * @param key
     * @param value
     * @param duration
     */
    void insert(const K& key, const V& value, Duration duration) {
      std::lock_guard<decltype(mutex)> lockGuard(mutex);
//      TimePoint expirationTime = std::chrono::steady_clock::now() + expiration;
      auto valIt = expirableItems.emplace(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(value, expirableTimes.end()));
      if (!valIt.second) {
        // cppreference.com: The element may be constructed even if there already is an element with the key in the container, in which case the newly constructed element will be destroyed immediately.
        throw std::runtime_error("Attempt to replace a key in ExpirationMap. Not implemented yet."); // TODO: implement if needed
      }
//      if (valIt.second) {
//        // The value was already present, delete old expiration time:
//        expirableTimes.erase(valIt.first->second.expirationTimeIt);
//      }
//      // Update expiration time:
//      auto timeIt = expirableTimes.emplace(expirationTime, &(valIt.first->first));
//      valIt.first->second.expirationTimeIt = timeIt;
      keepAlive_nomutex(valIt.first, duration);
    }
    
    /**
     * Remove an item from the map
     * @param key
     */
    void erase(const K& key) {
      std::lock_guard<decltype(mutex)> lockGuard(mutex);
      auto itemIt = expirableItems.find(key);
      erase_nomutex(itemIt);
    }
    
    /**
     * Get stored element
     * @param key
     * @return 
     */
    typename std::experimental::optional<V> get(const K& key) {
      std::lock_guard<decltype(mutex)> lockGuard(mutex);
      auto it = expirableItems.find(key);
      if (it != expirableItems.end()) {
        return std::experimental::make_optional(it->second.value);
      }
      return {};
    }
    
    /**
     * Get stored element. If the item exists, reset expiration to now() + expirationDuration
     * @param key
     * @param expirationDuration
     * @return 
     */
    typename std::experimental::optional<V> get(const K& key, Duration expirationDuration) {
      std::lock_guard<decltype(mutex)> lockGuard(mutex);
      auto itemIt = expirableItems.find(key);
      if (keepAlive_nomutex(itemIt, expirationDuration)) {
        return std::experimental::make_optional(itemIt->second.value);
      }
      return {};
    }
    
    /**
     * The nearest TimePoint at which an item will expire (can be in the past if the map contains expired items)
     * @return 
     */
    const TimePoint getNextExpirationTime() {
      std::lock_guard<decltype(mutex)> lockGuard(mutex);
      if (expirableTimes.empty())
        return TimePoint::max();
      return expirableTimes.begin()->expirationTime;
    }
    
    /**
     * Reset the expiration of an item to now() + dur
     * @param key
     * @param dur
     * @return true if item with given key exists, false otherwise
     */
    bool keepAlive(const K& key, Duration dur) {
      std::lock_guard<decltype(mutex)> lockGuard(mutex);
      auto itemIt = expirableItems.find(key);
      return keepAlive_nomutex(itemIt, dur);
    }

    /**
     * Remove all expired items from the map and return them
     * @return 
     */
    ExpiredValues pop_expired() {
      std::lock_guard<decltype(mutex)> lockGuard(mutex);
      ExpiredValues expired;
      auto endIt = expirableTimes.upper_bound({std::chrono::steady_clock::now(), {}}); // TODO: optimize constructor overhead
      for (auto it = expirableTimes.begin(); it != endIt; ) {
        const K& key = *(it->itemKey);
        ++it; // advance to next expirableTime before deletion of the current one
        auto itemIt = expirableItems.find(key);
        expired.insert({key, std::move(itemIt->second.value)});
        erase_nomutex(itemIt);
      }
      return expired;
    }


  private:
    struct ExpirableTime {
      ExpirableTime(const TimePoint& expirationTime, const K* itemKey) :
          expirationTime(expirationTime), itemKey(itemKey) {}
      TimePoint expirationTime;
      const K* itemKey;
    };
    struct ExpirableTimeComparator {
      bool operator()(const ExpirableTime& lhs, const ExpirableTime& rhs) const {
        return lhs.expirationTime < rhs.expirationTime;
      }
    };
    using ExpirableTimes = typename std::multiset<ExpirableTime, ExpirableTimeComparator>;

    struct ExpirableValue {
      ExpirableValue(const V& value, const typename ExpirableTimes::iterator& expirationTime) :
          value(value), expirationTimeIt(expirationTime) {}
      V value;
      typename ExpirableTimes::iterator expirationTimeIt;
    };
    using ExpirableItems = typename std::map<K, ExpirableValue>;

  private:
    void erase_nomutex(typename ExpirableItems::iterator itemIt) {
      if (itemIt == expirableItems.end())
        return;
      auto timeIt = itemIt->second.expirationTimeIt;
      expirableTimes.erase(timeIt);
      expirableItems.erase(itemIt);
    }

    /**
     * Reset an item's expiration
     * @param itemIt
     * @param dur
     * @return true if item exists
     */
    bool keepAlive_nomutex(typename ExpirableItems::iterator itemIt, Duration dur) {
      if (itemIt != expirableItems.end()) {
        TimePoint expirationTime = std::chrono::steady_clock::now() + dur;
        auto expirationTimeIt = itemIt->second.expirationTimeIt;
        if (expirationTimeIt != expirableTimes.end()) {
          expirableTimes.erase(expirationTimeIt);
        }
        expirationTimeIt = expirableTimes.emplace(expirationTime, &(itemIt->first));
        itemIt->second.expirationTimeIt = expirationTimeIt;
        return true;
      }
      return false;
    }
    
    mutable std::mutex mutex;
    ExpirableItems expirableItems;
    ExpirableTimes expirableTimes;
  };

  template <typename K, typename V>
  class PeriodicExpirator {
    public:
      using SharedExpirationMap = std::shared_ptr<ExpirationMap<K,V>>;
      using ExpirationCallback = std::function<void(std::map<K,V>&&)>; // TODO: use defined types
      /**
       * 
       * @param sharedExpirationMap The set to check for expired elements
       * @param checkInterval Time interval in which to check for expiration
       * @param callback Callback to call with expired items
       */
      PeriodicExpirator(SharedExpirationMap sharedExpirationMap, Duration checkInterval, ExpirationCallback callback) :
              sharedExpirationMap(sharedExpirationMap),
              checkInterval(checkInterval),
              stopFlag(false),
              callback(callback),
              worker(std::bind(&PeriodicExpirator::workerLoop, this)) {
        if (!this->callback)
          throw std::runtime_error("Invalid callback supplied");
      }
      virtual ~PeriodicExpirator() {
        stopWorker();
      }

    private:
      void workerLoop() {
        std::unique_lock<decltype(mutex)> lock(mutex);
        while (!stopFlag) {
          // Check if there are expired values in the expiration set:
          if(sharedExpirationMap->getNextExpirationTime() <= std::chrono::steady_clock::now()) {
            callback(std::move(sharedExpirationMap->pop_expired()));
          }
          stopCondition.wait_for(lock, checkInterval);
        }
      }
      
      void stopWorker(bool waitUntilStopped = true) {
        {
          std::lock_guard<decltype(mutex)> lockGuard(mutex);
          stopFlag = true;
          stopCondition.notify_one();
        }
        if (waitUntilStopped)
          worker.join(); // Wait for the thread 
      }
      
      mutable std::mutex mutex;
      SharedExpirationMap sharedExpirationMap;
      Duration checkInterval;
      bool stopFlag;
      ExpirationCallback callback;
      std::condition_variable stopCondition;
      std::thread worker;
  };
  
} // namespace
