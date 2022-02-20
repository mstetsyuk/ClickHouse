#include <Processors/Transforms/CachingTransform.h>

namespace DB
{

void CachingTransform::transform(Chunk & chunk)
{
    cache[query_ptr->getTreeHash()].push_back(&chunk);
}

};
