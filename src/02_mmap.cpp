#include <algorithm>
#include <fcntl.h>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <ranges>
#include <spanstream>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <system_error>
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
    uint64_t cnt;
    double sum;

    float min;
    float max;
};

using DB = std::unordered_map<std::string, Record>;

DB process_input(std::istream &in) {
    DB db;

    std::string station;
    std::string value;

    // Grab the station and the measured value from the input
    while (std::getline(in, station, ';') && std::getline(in, value, '\n')) {
        // Convert the measured value into a floating point
        float fp_value = std::stof(value);

        // Lookup the station in our database
        auto it = db.find(station);
        if (it == db.end()) {
            // If it's not there, insert
            db.emplace(station, Record{1, fp_value, fp_value, fp_value});
            continue;
        }
        // Otherwise update the information
        it->second.min = std::min(it->second.min, fp_value);
        it->second.max = std::max(it->second.max, fp_value);
        it->second.sum += fp_value;
        ++it->second.cnt;
    }

    return db;
}

void format_output(std::ostream &out, const DB &db) {
    std::vector<std::string> names(db.size());
    // Grab all the unique station names
    std::ranges::copy(db | std::views::keys, names.begin());
    // Sorting UTF-8 strings lexicographically is the same
    // as sorting by codepoint value
    std::ranges::sort(names);

    std::string delim = "";

    out << std::setiosflags(out.fixed | out.showpoint) << std::setprecision(1);
    out << "{";
    for (auto &k : names) {
        // Print StationName:min/avg/max
        auto &[_, record] = *db.find(k);
        out << std::exchange(delim, ", ") << k << "=" << record.min << "/"
            << (record.sum / record.cnt) << "/" << record.max;
    }
    out << "}\n";
}

int main() {
    MappedFile mfile("measurements.txt");
    std::ispanstream ifile(mfile.data());

    auto db = process_input(ifile);
    format_output(std::cout, db);
}