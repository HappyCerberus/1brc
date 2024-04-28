#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <ranges>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

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
    std::ifstream ifile("measurements.txt", ifile.in);
    if (not ifile.is_open())
        throw std::runtime_error("Failed to open the input file.");

    auto db = process_input(ifile);
    format_output(std::cout, db);
}
