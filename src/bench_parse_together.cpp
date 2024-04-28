#include <algorithm>
#include <benchmark/benchmark.h>
#include <cstring>
#include <fstream>
#include <iterator>
#include <span>
#include <string_view>
#include <vector>

struct Measurement {
    std::string_view name;
    uint16_t hash;
    int16_t value;
};

int16_t parse_int_base(std::span<const char>::iterator &iter) {
    bool negative = (*iter == '-');
    int16_t value = 0;
    while (*iter != '\n') {
        if (*iter != '.') {
            value *= 10;
            value += *iter - '0';
        }
        ++iter;
    }
    ++iter;
    if (negative)
        return -value;
    return value;
}

Measurement parse_base(std::span<const char>::iterator &iter) {
    Measurement result;

    auto end = std::ranges::find(iter, std::unreachable_sentinel, ';');
    result.name = {iter.base(), end.base()};
    result.hash = std::hash<std::string_view>{}(result.name);
    iter = end + 1;

    result.value = parse_int_base(iter);

    return result;
}

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

Measurement parse_v1(std::span<const char>::iterator &iter) {
    Measurement result;

    auto end = std::ranges::find(iter, std::unreachable_sentinel, ';');
    result.name = {iter.base(), end.base()};
    result.hash = std::hash<std::string_view>{}(result.name);
    iter = end + 1;

    result.value = parse_int_table(iter);

    return result;
}

Measurement parse_v2(std::span<const char>::iterator &iter) {
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

static void BM_parse_base(benchmark::State &state) {
    std::vector<char> data;
    {
        std::ifstream in("trunc.txt");
        data.insert(data.end(), std::istreambuf_iterator<char>(in),
                    std::istreambuf_iterator<char>());
    }
    std::span<const char> test_data(data);

    for (auto _ : state) {
        auto iter = test_data.begin();
        while (iter != test_data.end()) {
            auto station = parse_base(iter);
            benchmark::DoNotOptimize(station);
        }
    }
}
BENCHMARK(BM_parse_base);

static void BM_parse_v1(benchmark::State &state) {
    std::vector<char> data;
    {
        std::ifstream in("trunc.txt");
        data.insert(data.end(), std::istreambuf_iterator<char>(in),
                    std::istreambuf_iterator<char>());
    }
    std::span<const char> test_data(data);

    for (auto _ : state) {
        auto iter = test_data.begin();
        while (iter != test_data.end()) {
            auto station = parse_v1(iter);
            benchmark::DoNotOptimize(station);
        }
    }
}
BENCHMARK(BM_parse_v1);

static void BM_parse_v2(benchmark::State &state) {
    std::vector<char> data;
    {
        std::ifstream in("trunc.txt");
        data.insert(data.end(), std::istreambuf_iterator<char>(in),
                    std::istreambuf_iterator<char>());
    }
    std::span<const char> test_data(data);

    for (auto _ : state) {
        auto iter = test_data.begin();
        while (iter != test_data.end()) {
            auto station = parse_v2(iter);
            benchmark::DoNotOptimize(station);
        }
    }
}
BENCHMARK(BM_parse_v2);

static void BM_noop(benchmark::State &state) {
    for (auto _ : state) {
    }
}
BENCHMARK(BM_noop);

BENCHMARK_MAIN();
