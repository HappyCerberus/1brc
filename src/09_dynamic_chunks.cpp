#include <algorithm>
#include <cstdint>
#include <fcntl.h>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <ranges>
#include <span>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <utility>
#include <vector>

// A move-only helper
template <typename T, T empty = T{}> struct MoveOnly {
    MoveOnly() : store_(empty) {}
    MoveOnly(T value) : store_(value) {}
    MoveOnly(MoveOnly &&other) : store_(std::exchange(other.store_, empty)) {}
    MoveOnly &operator=(MoveOnly &&other) {
        store_ = std::exchange(other.store_, empty);
        return *this;
    }
    operator T() const { return store_; }
    T get() const { return store_; }

  private:
    T store_;
};

struct FileFD {
    FileFD(const std::filesystem::path &file_path)
        : fd_(open(file_path.c_str(), O_RDONLY)) {
        if (fd_ == -1)
            throw std::system_error(errno, std::system_category(),
                                    "Failed to open file");
    }

    ~FileFD() {
        if (fd_ >= 0)
            close(fd_);
    }

    int get() const { return fd_.get(); }

  private:
    MoveOnly<int, -1> fd_;
};

struct MappedFile {
    MappedFile(const std::filesystem::path &file_path) : fd_(file_path) {
        // Determine the filesize (needed for mmap)
        struct stat sb;
        if (fstat(fd_.get(), &sb) == 1)
            throw std::system_error(errno, std::system_category(),
                                    "Failed to read file stats");
        sz_ = sb.st_size;

        begin_ = static_cast<char *>(
            mmap(NULL, sz_, PROT_READ, MAP_PRIVATE, fd_.get(), 0));
        if (begin_ == MAP_FAILED)
            throw std::system_error(errno, std::system_category(),
                                    "Failed to map file to memory");
        chunk_begin_ = begin_;
    }

    ~MappedFile() {
        if (begin_ != nullptr)
            munmap(begin_, sz_);
    }

    std::span<const char> next_chunk() {
        std::lock_guard lock{mux_};
        if (chunk_begin_ == begin_ + sz_)
            return {};

        size_t chunk_sz = 64 * 1024 * 1024; // 64MB

        const char *end = nullptr;
        // prevent reading past the end of the file
        if (chunk_begin_ + chunk_sz > begin_ + sz_) {
            end = begin_ + sz_;
        } else {
            end = chunk_begin_ + chunk_sz;
            while (end != begin_ + sz_ && *end != '\n')
                ++end;
            ++end;
        }
        std::span<const char> result{chunk_begin_, end};
        chunk_begin_ = end;
        return result;
    }

  private:
    FileFD fd_;
    MoveOnly<char *> begin_;
    MoveOnly<size_t> sz_;
    const char *chunk_begin_;
    std::mutex mux_;
};

struct Measurement {
    std::string_view name;
    uint16_t hash;
    int16_t value;
};

struct Record {
    int64_t cnt;
    int64_t sum;

    int16_t min;
    int16_t max;
};

struct DB {
    DB() : keys_{}, values_{}, filled_{} {}

    void record(const Measurement &record) {
        // Find the slot for this station
        size_t slot = lookup_slot(record);

        // If the slot is empty, we have a miss
        if (keys_[slot].empty()) {
            filled_.push_back(slot);
            keys_[slot] = record.name;
            values_[slot] = Record{1, record.value, record.value, record.value};
            return;
        }

        // Otherwise we have a hit
        if (record.value < values_[slot].min)
            values_[slot].min = record.value;
        else if (record.value > values_[slot].max)
            values_[slot].max = record.value;
        values_[slot].sum += record.value;
        ++values_[slot].cnt;
    }

    size_t lookup_slot(const Measurement &record) const {
        uint16_t slot = record.hash;

        // While the slot is already occupied
        while (not keys_[slot].empty()) {
            // If it is the same name, we have a hit
            if (keys_[slot] == record.name)
                break;
            // Otherwise we have a collision
            ++slot;
        }

        // Either the first empty slot or a hit
        return slot;
    }

    // Keys
    std::array<std::string, UINT16_MAX> keys_;
    // Values
    std::array<Record, UINT16_MAX> values_;
    // Record of used indices (needed for output)
    std::vector<size_t> filled_;
};

consteval auto int_parse_table() {
    std::array<std::array<int16_t, 2>, 256> data;
    for (size_t c = 0; c < 256; ++c) {
        if (c >= '0' && c <= '9') {
            data[c][0] = c - '0';
            data[c][1] = 10;
        } else {
            data[c][0] = 0;
            data[c][1] = 1;
        }
    }
    return data;
}

static constexpr auto params = int_parse_table();

int16_t parse_int_table(std::span<const char>::iterator &iter) {
    char sign = *iter;
    int16_t result = 0;
    while (*iter != '\n') {
        result *= params[*iter][1];
        result += params[*iter][0];
        ++iter;
    }
    ++iter;
    if (sign == '-')
        return result * -1;
    return result;
}

Measurement parse(std::span<const char>::iterator &iter) {
    Measurement result;

    const char *begin = iter.base();
    result.hash = 0;
    while (*iter != ';') {
        result.hash = result.hash * 7 + *iter;
        ++iter;
    }
    result.name = {begin, iter.base()};
    ++iter;

    result.value = parse_int_table(iter);

    return result;
}

void process_input(DB &db, std::span<const char> data) {
    auto iter = data.begin();

    while (iter != data.end()) {
        // Scan for the end of the station name
        auto record = parse(iter);

        db.record(record);
    }
}

std::unordered_map<std::string, Record> process_parallel(MappedFile &file,
                                                         size_t chunks) {
    // Process the chunks in separate thread each
    std::vector<std::jthread> runners(chunks);
    std::vector<DB> dbs(chunks);
    for (size_t i = 0; i < chunks; ++i) {
        runners[i] = std::jthread([&, idx = i]() {
            auto chunk = file.next_chunk();
            while (not chunk.empty()) {
                process_input(dbs[idx], chunk);
                chunk = file.next_chunk();
            }
        });
    }
    runners.clear(); // join threads

    // Merge the partial DBs
    std::unordered_map<std::string, Record> merged;
    for (auto &db_chunk : dbs) {
        for (auto idx : db_chunk.filled_) {
            auto it = merged.find(db_chunk.keys_[idx]);
            if (it == merged.end()) {
                merged.insert_or_assign(db_chunk.keys_[idx],
                                        db_chunk.values_[idx]);
            } else {
                it->second.cnt += db_chunk.values_[idx].cnt;
                it->second.sum += db_chunk.values_[idx].sum;
                it->second.max =
                    std::max(it->second.max, db_chunk.values_[idx].max);
                it->second.min =
                    std::min(it->second.min, db_chunk.values_[idx].min);
            }
        }
    }
    return merged;
}

void format_output(std::ostream &out,
                   std::unordered_map<std::string, Record> &db) {
    std::vector<std::string> names(db.size());
    // Grab all the unique station names
    std::ranges::copy(db | std::views::keys, names.begin());
    // Sorting UTF-8 strings lexicographically is the same
    // as sorting by codepoint value
    std::ranges::sort(names, std::less<>{});

    std::string delim = "";

    out << std::setiosflags(out.fixed | out.showpoint) << std::setprecision(1);
    out << "{";
    for (auto &name : names) {
        auto &value = db[name];

        int64_t sum = value.sum;
        // Correct rounding
        if (sum > 0)
            sum += value.cnt / 2;
        else
            sum -= value.cnt / 2;
        out << std::exchange(delim, ", ") << name << "=" << value.min / 10.0
            << "/" << (sum / value.cnt) / 10.0 << "/" << value.max / 10.0;
    }
    out << "}\n";
}

int main(int argc, char **argv) {
    size_t chunks = 1;
    if (argc == 2) {
        chunks = atol(argv[1]);
    }
    MappedFile mfile("measurements.txt");

    auto db = process_parallel(mfile, chunks);
    format_output(std::cout, db);
}