#pragma once
#include <unordered_map>
#include <Processors/QueryPlan/ITransformingStep.h>
#include <Parsers/IAST.h>
#include <Interpreters/InterpreterSelectQuery.h>
#include <Common/LRUCache.h>
#include <Storages/QueryCache.h>


namespace DB
{

class CachingStep : public ITransformingStep
{
public:
    CachingStep(const DataStream & input_stream_, QueryCachePtr cache_, CacheKey cache_key_);
    ~CachingStep() override;

    String getName() const override { return "Caching"; }

    void transformPipeline(QueryPipelineBuilder & pipeline, const BuildQueryPipelineSettings &) override;

private:
    QueryCachePtr cache;
    CacheKey cache_key;
    bool execute_caching = false;
};

}
