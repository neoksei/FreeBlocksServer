//
// Created by hivemind on 16/05/19.
//
#ifndef FREEBLOCKS_FILESYSTEM_H
#define FREEBLOCKS_FILESYSTEM_H

#include <climits>
#include <cmath>
#include <exception>
#include <cstring>
#include <fstream>
#include <algorithm>

#define CLUSTER_FREE SIZE_MAX - 1
#define LAST_CLUSTER SIZE_MAX
#define RECORD_FREE SIZE_MAX
#define FS_FAIL SIZE_MAX


struct Record {
    char name[16];
    size_t index;
    size_t size;
};


class FileSystem {
private:
    size_t n_clusters;
    size_t cluster_size;
    size_t total_clusters_busy;
    char *storage;
    Record root_file[512];

    size_t allocate_clusters(size_t amount);

    size_t get_real_size(size_t data_size);

    size_t calculate_nclusters(size_t data_size);

    size_t save_data(size_t init_cluster, const char *data, size_t data_size);

    size_t safe_to_cluster(size_t index, const char *data, size_t data_size);

public:

    FileSystem(const char* dump_file, size_t _cluster_size);

    FileSystem(size_t _cluster_size, size_t _n_clusters);

    ~FileSystem();

    bool file_exists(const char *name);

    size_t write(const char *name, const char *file_data, size_t file_size);

    size_t delete_file(const char *name);

    char *read(const char *name);

    size_t get_file_size(const char *name);

    void dump();
};

#endif //FREEBLOCKS_FILESYSTEM_H
