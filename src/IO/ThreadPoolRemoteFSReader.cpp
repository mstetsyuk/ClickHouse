#include <IO/ThreadPoolRemoteFSReader.h>

#include <Common/Exception.h>
#include <Common/ProfileEvents.h>
#include <Common/CurrentMetrics.h>
#include <Common/Stopwatch.h>
#include <Common/assert_cast.h>
#include <Common/setThreadName.h>

#include <IO/SeekableReadBuffer.h>
#include <Disks/ReadBufferFromRemoteFSGather.h>

#include <future>
#include <iostream>
#include <common/logger_useful.h>


namespace ProfileEvents
{
    extern const Event RemoteFSReadMicroseconds;
    extern const Event RemoteFSReadBytes;
}

namespace CurrentMetrics
{
    extern const Metric Read;
}

namespace DB
{

size_t ThreadPoolRemoteFSReader::RemoteFSFileDescriptor::readInto(char * data, size_t size, size_t offset)
{
    return reader->readInto(data, size, offset);
}


ThreadPoolRemoteFSReader::ThreadPoolRemoteFSReader(size_t pool_size, size_t queue_size_)
    : pool(pool_size, pool_size, queue_size_)
{
}


std::future<IAsynchronousReader::Result> ThreadPoolRemoteFSReader::submit(Request request)
{
    auto task = std::make_shared<std::packaged_task<Result()>>([request]
    {
        setThreadName("ThreadPoolRead");
        CurrentMetrics::Increment metric_increment{CurrentMetrics::Read};
        auto * remote_fs_fd = assert_cast<RemoteFSFileDescriptor *>(request.descriptor.get());

        Stopwatch watch(CLOCK_MONOTONIC);
        auto bytes_read = remote_fs_fd->readInto(request.buf, request.size, request.offset);
        watch.stop();

        ProfileEvents::increment(ProfileEvents::RemoteFSReadMicroseconds, watch.elapsedMicroseconds());
        ProfileEvents::increment(ProfileEvents::RemoteFSReadBytes, bytes_read);

        return bytes_read;
    });

    auto future = task->get_future();

    /// ThreadPool is using "bigger is higher priority" instead of "smaller is more priority".
    pool.scheduleOrThrow([task]{ (*task)(); }, -request.priority);

    return future;
}
}
