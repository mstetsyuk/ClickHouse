if (TARGET ch_contrib::rocksdb)
    set(USE_ROCKSDB 1)
endif()
if (TARGET ch_contrib::bzip2)
    set(USE_BZIP2 1)
endif()
if (TARGET ch_contrib::snappy)
    set(USE_SNAPPY 1)
endif()
if (TARGET ch_contrib::hivemetastore)
    set(USE_HIVE 1)
endif()
