//
// Created by hivemind on 16/05/19.
//

#include "FileSystem.h"

FileSystem::FileSystem(size_t _cluster_size, size_t _n_clusters) {
    cluster_size = _cluster_size;
    n_clusters = _n_clusters;
    total_clusters_busy = 0;
    storage = new char[n_clusters * cluster_size]();

    for (size_t i = 0; i < n_clusters; i++) {
        *((size_t *) (storage + i * cluster_size)) = CLUSTER_FREE;
    }

    for (size_t i = 1; i < 512; i++) {
        root_file[i].index = RECORD_FREE;
        root_file[i].size = 0;
        std::fill(root_file[i].name, root_file[i].name + sizeof(Record::name), 0);
    }

    strcpy(root_file[0].name, "/");

    root_file[0].size = sizeof(root_file);
    root_file[0].index = allocate_clusters(calculate_nclusters(sizeof(root_file)));
}

FileSystem::FileSystem(const char *dump_file, size_t _cluster_size) {
    std::ifstream dump_stream(dump_file);
    dump_stream.seekg(0, std::ifstream::end);
    size_t length = dump_stream.tellg();
    dump_stream.seekg(0, std::ifstream::beg);
    cluster_size = _cluster_size;
    n_clusters = length / cluster_size;
    storage = new char[n_clusters * cluster_size];
    total_clusters_busy = 0;

    dump_stream.read(storage, length);
    dump_stream.close();

    for (size_t i = 0; i < 512 * sizeof(Record); i++) { //инициализация root_file

        if (i % cluster_size == 0) {
            i += sizeof(size_t);
        } else {
            *((char *) root_file + i) = storage[i];
        }
    }

    for (size_t i = 0; i < n_clusters; i++) {

        if (*(size_t *) (storage + i * cluster_size) != CLUSTER_FREE) {
            ++total_clusters_busy;
        }
    }

}

FileSystem::~FileSystem() {
    delete[] storage;
}

size_t FileSystem::allocate_clusters(size_t amount) {
    size_t total_allocated = 0;
    size_t storage_offset;
    size_t cluster_status;
    size_t init_cluster_index;
    size_t pointer_offset;
    size_t current_cluster_index;

    for (size_t i = 0; i < n_clusters; i++) {

        if (total_allocated == amount) {
            total_clusters_busy += total_allocated;
            break;
        }

        storage_offset = i * cluster_size;
        cluster_status = *(size_t *) (storage + storage_offset);

        if (cluster_status == CLUSTER_FREE) {
            ++total_allocated;

            if (total_allocated == 1) {
                init_cluster_index = storage_offset / cluster_size;
                pointer_offset = storage_offset;

            } else {
                current_cluster_index = storage_offset / cluster_size;
                *(size_t *) (storage + pointer_offset) = current_cluster_index;
                pointer_offset = storage_offset;

            }
        }


    }

    *(size_t *) (storage + pointer_offset) = LAST_CLUSTER;

    return init_cluster_index;
}

size_t FileSystem::get_real_size(size_t data_size) {
    return (size_t) ceil((double) data_size / (double) (cluster_size - sizeof(size_t))) * sizeof(size_t) + data_size;
}

size_t FileSystem::calculate_nclusters(size_t data_size) {
    return (size_t) ceil((double) get_real_size(data_size) / (double) cluster_size);
}

size_t FileSystem::save_data(size_t init_cluster, const char *data, size_t data_size) {
    size_t saved_bytes = 0;
    size_t current_index = init_cluster;

    while (saved_bytes < data_size) {
        saved_bytes += safe_to_cluster(current_index, data + saved_bytes, data_size - saved_bytes);
        current_index = *(size_t *) (storage + current_index * cluster_size);
    }

    return saved_bytes;
}

size_t FileSystem::safe_to_cluster(size_t index, const char *data, size_t data_size) {
    size_t storage_offset = index * cluster_size + sizeof(size_t);
    memcpy((void *) (storage + storage_offset), (void *) data, std::min(data_size, cluster_size - sizeof(size_t)));
    return (size_t) fmin(data_size, cluster_size - sizeof(size_t));
}

size_t FileSystem::write(const char *name, const char *file_data, size_t file_size) {
    char *buffer = nullptr;
    size_t buffer_size;

    if (strlen(name) >= 16) { // неподходящее имя
        return FS_FAIL;
    }

    if (file_exists(name)) {
        buffer = read(name);
        buffer_size = get_file_size(name);
        delete_file(name);
    }

    if (calculate_nclusters(file_size) > n_clusters - total_clusters_busy) { // закончились кластеры

        if (buffer != nullptr) { // файл не удаляется, просто не изменяется
            write(name, buffer, buffer_size);
            delete[] buffer;
        }

        return FS_FAIL;
    }

    size_t file_index = SIZE_MAX;

    for (size_t i = 0; i < sizeof(root_file) / sizeof(Record); i++) { // поиск свободной записи в root_file

        if (root_file[i].index == RECORD_FREE) {
            file_index = i;
            break;
        }
    }

    if (file_index == SIZE_MAX) { // достигнут предел количества файлов
        return FS_FAIL;

    } else {
        strcpy(root_file[file_index].name, name); // инициализация записи в root_file
        root_file[file_index].size = file_size;
        root_file[file_index].index = allocate_clusters(calculate_nclusters(file_size));
        save_data(root_file[file_index].index, file_data, file_size);
        delete[] buffer; //очищаем буфер
        return file_index;
    }
}

size_t FileSystem::delete_file(const char *name) {
    size_t current_index = SIZE_MAX;
    size_t next_index;
    size_t deleted_size = 0;

    for (Record &x: root_file) { // ищем файл
        if (!strcmp(name, x.name)) {
            current_index = x.index; // сохраняем индекс
            x.index = RECORD_FREE; // файл удален в root_file
            deleted_size = x.size;
            break;
        }
    }

    if (current_index == SIZE_MAX) { // запись не найдена
        return FS_FAIL;
    }


    next_index = *(size_t *) (storage + current_index * cluster_size);
    while (next_index != LAST_CLUSTER) {


        for (size_t i = sizeof(size_t); i < cluster_size; i++) {
            storage[current_index * cluster_size + i] = 0; // полностью зануляет все кластеры
        }                                                  // связанные с файлом


        next_index = *(size_t *) (storage + current_index * cluster_size);
        *((size_t *) (storage + current_index * cluster_size)) = CLUSTER_FREE;
        current_index = next_index;
        --total_clusters_busy;
    }

    *((size_t *) (storage + current_index * cluster_size)) = CLUSTER_FREE;

    for (size_t i = sizeof(size_t); i < cluster_size; i++) {
        storage[current_index * cluster_size + i] = 0;
    }

    --total_clusters_busy;

    save_data(root_file[0].index, (char *) root_file, sizeof(root_file)); // обновляем root_file
    return deleted_size;

}

char *FileSystem::read(const char *name) {
    size_t current_index = SIZE_MAX;
    Record record;

    for (Record x: root_file) { // поиск записи в root_file
        if (!strcmp(name, x.name)) {
            record = x;
            current_index = record.index;
            break;
        }
    }

    if (current_index == SIZE_MAX) { // файл не найден
        return nullptr;
    }

    size_t balance = record.size;
    char *buffer = new char[record.size];
    size_t buffer_size = record.size;

    while (current_index != LAST_CLUSTER) { // заполняется выделенный буфер

        for (size_t i = sizeof(size_t); i < cluster_size && balance > 0; i++, balance--) {
            buffer[buffer_size - balance] = storage[cluster_size * current_index + i];
        }

        if (!balance) { // буфер заполнен
            return buffer;
        }

        current_index = *(size_t *) (storage + current_index * cluster_size);
    }

    for (size_t i = sizeof(size_t); i < cluster_size && balance > 0; i++, balance--) { // дозапись с последнего кластера
        buffer[buffer_size - balance] = storage[cluster_size * current_index + i];
    }

    return buffer;

}

size_t FileSystem::get_file_size(const char *name) {

    for (Record x: root_file) { // поиск записи в root_file
        if (!strcmp(name, x.name)) {
            return x.size;
        }
    }
}

void FileSystem::dump() {
    save_data(root_file[0].index, (char *) root_file, sizeof(root_file));
    std::ofstream f = std::ofstream("../FreeDrive");
    f.write(storage, n_clusters * cluster_size);
    f.close();
}

bool FileSystem::file_exists(const char *name) {

    for (Record x: root_file) {
        if (!strcmp(name, x.name)) {
            return true;
        }
    }

    return false;
}
