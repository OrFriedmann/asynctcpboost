using namespace std;
#include <unordered_map>
#include <iostream>
#include <list>
#include <mutex>
#include <chrono>
class LRU {
    std::mutex lock;
    list<std::string> lru_queue;
    typedef pair<std::list<string>::iterator, std::chrono::time_point<std::chrono::steady_clock>> lru_pair;
    std::unordered_map<std::string, lru_pair> hash_map;
    size_t max_size;
    const uint8_t expiration = 60;

    public:
        LRU(size_t _max_size)
        {
            max_size = _max_size;
        }
        size_t size() {
            std::unique_lock<std::mutex> ulock(lock);
            return lru_queue.size();
        }
        size_t max_capacity() {
            return max_size;
        }
        bool get(std::string key)
        {
            std::unique_lock<std::mutex> ulock(lock);
            auto keyfind = hash_map.find(key);
            if(keyfind == hash_map.end()) {
                return false;
            }
            auto now = std::chrono::steady_clock::now();
            if(std::chrono::duration_cast<std::chrono::seconds>(now - keyfind->second.second).count() >= expiration)
            {
                lru_queue.erase(keyfind->second.first);
                hash_map.erase(keyfind);
                return false;
            }
            lru_queue.erase(keyfind->second.first);
            lru_queue.push_front(key);
            hash_map[key].first = lru_queue.begin();
            return true;
        }
        void put(std::string key)
        {
            std::unique_lock<std::mutex> ulock(lock);
            auto keyfind = hash_map.find(key);
            if(keyfind == hash_map.end())
            {
                if(lru_queue.size() == max_size)
                {
                    std::string evict = lru_queue.back();
                    lru_queue.pop_back();
                    hash_map.erase(evict);
                }
            }  else {
                lru_queue.erase(keyfind->second.first);
            }
            auto now = std::chrono::steady_clock::now();
            lru_queue.push_front(key);
            hash_map[key].first = lru_queue.begin();
            hash_map[key].second = now;
        }

};