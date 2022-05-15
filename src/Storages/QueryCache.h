#pragma once

#include <memory>

#include <Common/LRUCache.h>


namespace DB
{
using Data = std::pair<Block, Chunks>;

struct CacheKey
{
    CacheKey(ASTPtr ast_, const Block & header_, const Settings & settings_, const std::optional<String> & username_)
        : ast(ast_)
        , header(header_)
        , settings(settings_)
        , username(username_) {}

    bool operator==(const CacheKey & other) const
    {
        return ast->getTreeHash() == other.ast->getTreeHash() && header == other.header
            //               && settingsSet(settings) == settingsSet(other.settings)
            //               && username == other.username
            ;
    }

    ASTPtr ast;
    Block header;
    const Settings & settings;
    const std::optional<String> & username;

    //    static std::set<String> settingsSet(const Settings & settings) {
    //        std::set<String> res;
    //        for (const auto & s : settings.all()) {
    //            res.insert(s.getValueString());
    //        }
    //        return res;
    //    }
};

struct CacheKeyHasher
{
    size_t operator()(const CacheKey & k) const
    {
        auto ast_info = k.ast->getTreeHash();
        auto header_info = k.header.getNamesAndTypesList().toString();
        //        auto settings_info = settingsHash(k.settings);
        //        auto username_info = std::hash<std::optional<String>>{}(k.username);

        return ast_info.first + ast_info.second * 9273 + std::hash<String>{}(header_info)*9273 * 9273
            //            + settings_info * 9273 * 9273 * 9273
            //              + username_info * 9273 * 9273 * 9273 * 9273
            ;
    }
    //private:
    //    static size_t settingsHash(const Settings & settings) {
    //        size_t hash = 0;
    //        size_t coefficient = 1;
    //        for (const auto & s : settings) {
    //            hash += std::hash<String>{}(s.getValueString()) * coefficient;
    //            coefficient *= 53;
    //        }
    //        return hash;
    //    }
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


class QueryCache : public LRUCache<CacheKey, Data, CacheKeyHasher, QueryWeightFunction>
{
private:
    using Base = LRUCache<CacheKey, Data, CacheKeyHasher, QueryWeightFunction>;

public:
    QueryCache(size_t cache_size_in_bytes)
        : Base(cache_size_in_bytes)
    {

    }

    void addChunk(CacheKey cache_key, Chunk && chunk)
    {
        auto data = get(cache_key);
        data->second.push_back(std::move(chunk));
//        if (weight(*data) > max_query_cache_entry_size) {
//             remove the entry from cache + make sure the subsequent chunks will not create it again
//        }
        set(cache_key, data); // evicts cache if necessary
    }

    size_t recordQueryRun(CacheKey cache_key)
    {
        std::lock_guard lock(times_executed_mutex);
        return ++times_executed[cache_key];
    }

    std::mutex& getPutInCacheMutex(CacheKey cache_key) {
        return put_in_cache_mutexes[cache_key];
    }

    void scheduleRemoval(CacheKey cache_key, std::chrono::seconds milis=std::chrono::seconds{15})
    {
        std::async(&QueryCache::removeOnTimeout, this, cache_key, milis);
    }

private:
    void removeOnTimeout(CacheKey cache_key, std::chrono::seconds milis=std::chrono::seconds{15})
    {
        std::this_thread::sleep_for(milis);
        remove(cache_key);
    }

private:
//    size_t max_query_cache_entry_size;
    std::unordered_map<CacheKey, size_t, CacheKeyHasher> times_executed;
    std::mutex times_executed_mutex;
    std::unordered_map<CacheKey, std::mutex, CacheKeyHasher> put_in_cache_mutexes;
    std::mutex put_in_cache_mutex;
//    QueryWeightFunction weight;
};

using QueryCachePtr = std::shared_ptr<QueryCache>;

}
