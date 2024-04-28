#include <algorithm>
#include <cstdint>
#include <fcntl.h>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <span>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>
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
    }

    ~MappedFile() {
        if (begin_ != nullptr)
            munmap(begin_, sz_);
    }

    // The entire file content as a std::span
    std::span<const char> data() const { return {begin_.get(), sz_.get()}; }

  private:
    FileFD fd_;
    MoveOnly<char *> begin_;
    MoveOnly<size_t> sz_;
};

struct Record {
    int64_t cnt;
    int64_t sum;

    int16_t min;
    int16_t max;
};

struct StringHash {
    using is_transparent = void;

    size_t operator()(std::string_view txt) const {
        return std::hash<std::string_view>{}(txt);
    }
    size_t operator()(const std::string &txt) const {
        return std::hash<std::string>{}(txt);
    }
};

struct DB {
    DB() : keys_{}, values_{}, filled_{} {}

    void record(std::string_view station, int16_t value) {
        // Find the slot for this station
        size_t slot = lookup_slot(station);

        // If the slot is empty, we have a miss
        if (keys_[slot].empty()) {
            filled_.push_back(slot);
            keys_[slot] = station;
            values_[slot] = Record{1, value, value, value};
            return;
        }

        // Otherwise we have a hit
        if (value < values_[slot].min)
            values_[slot].min = value;
        else if (value > values_[slot].max)
            values_[slot].max = value;
        values_[slot].sum += value;
        ++values_[slot].cnt;
    }

    size_t lookup_slot(std::string_view station) const {
        // Get a hash of the name truncated to 16bit
        uint16_t slot = std::hash<std::string_view>{}(station);

        // While the slot is already occupied
        while (not keys_[slot].empty()) {
            // If it is the same name, we have a hit
            if (keys_[slot] == station)
                break;
            // Otherwise we have a collision
            ++slot;
        }

        // Either the first empty slot or a hit
        return slot;
    }

    // Helper method for the output formatting
    void sort_slots() {
        auto cmp = [this](size_t left, size_t right) {
            return keys_[left] < keys_[right];
        };
        std::ranges::sort(filled_.begin(), filled_.end(), cmp);
    }

    // Keys
    std::array<std::string, UINT16_MAX> keys_;
    // Values
    std::array<Record, UINT16_MAX> values_;
    // Record of used indices (needed for output)
    std::vector<size_t> filled_;
};

std::string_view parse_station(std::span<const char>::iterator &iter) {
    auto begin = iter;
    auto end = std::ranges::find(iter, std::unreachable_sentinel, ';');
    iter = end + 1;
    return {begin.base(), end.base()};
}

int16_t parse_value(std::span<const char>::iterator &iter) {
    bool negative = (*iter == '-');
    if (negative)
        ++iter;
    int16_t result = 0;
    while (*iter != '\n') {
        if (*iter != '.') {
            result *= 10;
            result += *iter - '0';
        }
        ++iter;
    }
    if (negative)
        result *= -1;
    ++iter;
    return result;
}

DB process_input(std::span<const char> data) {
    DB db;

    auto iter = data.begin();

    while (iter != data.end()) {
        // Scan for the end of the station name
        auto station = parse_station(iter);
        // Scan for the end of measured value
        auto value = parse_value(iter);

        db.record(station, value);
    }

    return db;
}

void format_output(std::ostream &out, DB &db) {
    db.sort_slots();

    std::string delim = "";

    out << std::setiosflags(out.fixed | out.showpoint) << std::setprecision(1);
    out << "{";
    for (auto &k : db.filled_) {
        auto &name = db.keys_[k];
        auto &value = db.values_[k];

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

int main() {
    MappedFile mfile("measurements.txt");

    auto db = process_input(mfile.data());
    format_output(std::cout, db);
}