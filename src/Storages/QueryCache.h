#pragma once

#include <memory>
#include <condition_variable>
#include <Common/LRUCache.h>


namespace DB
{
using QueryCachePtr = std::shared_ptr<QueryCache>;

using Data = std::pair<Block, Chunks>;

struct CacheKey
{
    CacheKey(ASTPtr ast_, const Block & header_, const Settings & settings_, std::optional<String> username_)
        : ast(ast_)
        , header(header_)
        , settings(settings_)
        , username(std::move(username_)) {}

    bool operator==(const CacheKey & other) const
    {
        return ast->getTreeHash() == other.ast->getTreeHash()
               && header == other.header
               && settings == other.settings
               && username == other.username;
    }

    ASTPtr ast;
    Block header;
    Settings settings;
    std::optional<String> username;
};

struct CacheKeyHasher
{
    size_t operator()(const CacheKey & key) const
    {
        auto ast_info = key.ast->getTreeHash();
        auto header_info = std::hash<String>{}(key.header.getNamesAndTypesList().toString());
        auto settings_info = settingsHash(key.settings);
        auto username_info = std::hash<std::optional<String>>{}(key.username);

        return ast_info.first + ast_info.second * 9273
               + header_info * 9273 * 9273
               + settings_info * 9273 * 9273 * 9273
               + username_info * 9273 * 9273 * 9273 * 9273;
    }

    private:
        static size_t settingsHash(const Settings & settings)
        {
            size_t hash = 0;
            size_t coefficient = 1;
            for (const auto & setting : settings)
            {
                hash += std::hash<String>{}(setting.getValueString()) * coefficient;
                coefficient *= 53;
            }
            return hash;
        }
};

struct QueryWeightFunction
{
    size_t operator()(const Data & data) const
    {
        const Block & block = data.first;
        const Chunks & chunks = data.second;

        size_t res = 0;
        for (const auto & chunk : chunks)
        {
            res += chunk.allocatedBytes();
        }
        res += block.allocatedBytes();

        return res;
    }
};

class CacheRemovalScheduler
{
private:
    using timestamp = std::chrono::time_point<std::chrono::high_resolution_clock>;
    using duration = std::chrono::high_resolution_clock::duration;
public:
    void scheduleRemoval(duration duration, CacheKey cache_key)
    {
        std::lock_guard lock(mutex);
        TimedCacheKey timer = {now() + duration, cache_key};
        queue.push(timer);
        if (queue.top() == timer)
        {
            timer_cv.notify_one();
        }
    }

    template <typename Cache>
    [[noreturn]] void processRemovalQueue(Cache * query_cache)
    {
        while (true)
        {
            std::unique_lock lock(mutex);
            const std::optional<TimedCacheKey> awaited_timer = nextTimer();

            timer_cv.wait_until(lock,
                                awaited_timer.has_value() ? awaited_timer->time : infinite_time,
                                [&]() { return awaited_timer != nextTimer() || (awaited_timer.has_value() && awaited_timer->time <= now()); }
            );

            if (awaited_timer.has_value() && awaited_timer->time <= now())
            {
                query_cache->remove(awaited_timer->cache_key);
                queue.pop();
            }
        }
    }


private:
    struct TimedCacheKey
    {
        TimedCacheKey(timestamp timestamp, CacheKey key)
            : time(timestamp)
            , cache_key(key)
        {}

        bool operator==(const TimedCacheKey& other) const
        {
            return time == other.time;
        }

        bool operator<(const TimedCacheKey& other) const
        {
            return time < other.time;
        }

        timestamp time;
        CacheKey cache_key;
    };

    std::optional<TimedCacheKey> nextTimer() const
    {
        if (queue.empty())
        {
            return std::nullopt;
        }
        return std::make_optional(queue.top());
    }

    static timestamp now()
    {
        return std::chrono::high_resolution_clock::now();
    }

    const timestamp infinite_time = timestamp::max();
    std::priority_queue<TimedCacheKey> queue;
    std::condition_variable timer_cv;
    std::mutex mutex;
};

class QueryCache : public LRUCache<CacheKey, Data, CacheKeyHasher, QueryWeightFunction>
{
private:
    using Base = LRUCache<CacheKey, Data, CacheKeyHasher, QueryWeightFunction>;

public:
    explicit QueryCache(size_t cache_size_in_bytes_, size_t max_query_cache_entry_size_)
        : Base(cache_size_in_bytes_)
        , max_query_cache_entry_size(max_query_cache_entry_size_)
    {
        std::thread cache_removing_thread(&CacheRemovalScheduler::processRemovalQueue<QueryCache>, &removal_scheduler, this);
        cache_removing_thread.detach();
    }

    bool insertChunk(CacheKey cache_key, Chunk && chunk)
    {
        auto data = get(cache_key);
        data->second.push_back(std::move(chunk));

        if (query_weight(*data) > max_query_cache_entry_size)
        {
            remove(cache_key);
            return false;
        }
        set(cache_key, data); // evicts cache if necessary, the entry with key=cache_key will not get evicted
        return false;
    }

    void scheduleRemoval(CacheKey cache_key)
    {
        auto entry_put_timeout =  std::chrono::milliseconds{cache_key.settings.query_cache_entry_put_timeout};
        removal_scheduler.scheduleRemoval(entry_put_timeout, cache_key);
    }

    size_t recordQueryRun(CacheKey cache_key)
    {
        std::lock_guard lock(times_executed_mutex);
        return ++times_executed[cache_key];
    }

    std::mutex& getPutInCacheMutex(CacheKey cache_key)
    {
        return put_in_cache_mutexes[cache_key];
    }

private:
    CacheRemovalScheduler removal_scheduler;

    std::unordered_map<CacheKey, size_t, CacheKeyHasher> times_executed;
    std::mutex times_executed_mutex;

    QueryWeightFunction query_weight;
    size_t max_query_cache_entry_size;

    std::unordered_map<CacheKey, std::mutex, CacheKeyHasher> put_in_cache_mutexes;
};

}
