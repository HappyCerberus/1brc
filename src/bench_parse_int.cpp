#include <array>
#include <benchmark/benchmark.h>
#include <cstring>

int16_t parse_int_base(const char *&it) {
    bool negative = (*it == '-');
    int16_t value = 0;
    while (*it != '\n') {
        if (*it != '.') {
            value *= 10;
            value += *it - '0';
        }
        ++it;
    }
    ++it;
    if (negative)
        return -value;
    return value;
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

int16_t parse_int_table(const char *&iter) {
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

const char *test_data =
    "33.3\n9.9\n8.5\n15.4\n28.3\n19.3\n22.4\n2.6\n0.7\n48.2\n9.9\n9.3\n13."
    "6\n40.9\n30.3\n43.0\n24.3\n22.0\n27.1\n13.8\n12.1\n26.0\n37.7\n19.2\n22."
    "0\n36.4\n26.4\n10.9\n27.6\n20.3\n9.7\n24.9\n5.6\n37.4\n11.7\n20.1\n17."
    "6\n28.9\n7.3\n16.7\n-10.3\n18.9\n38.0\n37.5\n19.5\n25.2\n11.3\n17.1\n9."
    "4\n8.7\n26.0\n4.8\n9.2\n24.7\n5.0\n6.4\n9.8\n2.2\n38.9\n5.9\n21.9\n22.6\n-"
    "5.5\n29.4\n16.8\n8.9\n2.1\n37.8\n15.3\n10.7\n28.9\n-11.2\n19.4\n35.5\n12."
    "4\n7.0\n20.5\n32.9\n37.8\n27.8\n14.9\n24.2\n42.1\n32.3\n42.9\n18.0\n3."
    "0\n2.6\n-7.8\n2.3\n22.8\n20.1\n14.9\n14.7\n3.7\n34.9\n19.7\n26.1\n8.2\n-5."
    "4\n18.4\n4.3\n-4.3\n33.5\n1.5\n-29.3\n10.3\n6.5\n27.4\n21.5\n15.6\n30."
    "5\n11.1\n11.6\n14.4\n39.1\n22.8\n16.3\n-1.4\n33.1\n9.1\n20.6\n44.8\n5."
    "6\n11.3\n21.2\n11.8\n24.5\n4.2\n12.5\n16.6\n38.8\n35.1\n9.9\n20.2\n26."
    "3\n33.6\n11.6\n-0.8\n18.4\n10.1\n18.1\n13.8\n11.1\n34.1\n4.6\n9.7\n7."
    "0\n16.8\n34.2\n25.3\n20.1\n6.4\n1.9\n30.3\n7.1\n24.1\n11.3\n17.4\n33.8\n5."
    "2\n8.5\n31.2\n23.9\n11.4\n32.5\n12.4\n10.4\n17.9\n26.3\n16.7\n9.5\n18."
    "9\n39.3\n15.2\n8.9\n36.2\n-0.7\n11.6\n11.4\n21.7\n-10.9\n20.4\n-17.1\n30."
    "0\n31.4\n13.7\n5.9\n3.3\n14.8\n-1.3\n33.5\n48.6\n14.2\n-0.4\n9.6\n29."
    "9\n13.5\n25.8\n15.3\n";

static void BM_parse_int_base(benchmark::State &state) {
    const char *end = test_data + strlen(test_data);
    for (auto _ : state) {
        const char *iter = test_data;
        while (iter != end) {
            int16_t value = parse_int_base(iter);
            benchmark::DoNotOptimize(value);
        }
    }
}
BENCHMARK(BM_parse_int_base);

static void BM_parse_int_base_copy1(benchmark::State &state) {
    const char *end = test_data + strlen(test_data);
    for (auto _ : state) {
        const char *iter = test_data;
        while (iter != end) {
            int16_t value = parse_int_base(iter);
            benchmark::DoNotOptimize(value);
        }
    }
}
BENCHMARK(BM_parse_int_base_copy1);

static void BM_parse_int_base_copy2(benchmark::State &state) {
    const char *end = test_data + strlen(test_data);
    for (auto _ : state) {
        const char *iter = test_data;
        while (iter != end) {
            int16_t value = parse_int_base(iter);
            benchmark::DoNotOptimize(value);
        }
    }
}
BENCHMARK(BM_parse_int_base_copy2);

static void BM_parse_int_table(benchmark::State &state) {
    const char *end = test_data + strlen(test_data);
    for (auto _ : state) {
        const char *iter = test_data;
        while (iter != end) {
            int16_t value = parse_int_table(iter);
            benchmark::DoNotOptimize(value);
        }
    }
}
BENCHMARK(BM_parse_int_table);

static void BM_parse_int_table_copy1(benchmark::State &state) {
    const char *end = test_data + strlen(test_data);
    for (auto _ : state) {
        const char *iter = test_data;
        while (iter != end) {
            int16_t value = parse_int_table(iter);
            benchmark::DoNotOptimize(value);
        }
    }
}
BENCHMARK(BM_parse_int_table_copy1);

static void BM_parse_int_table_copy2(benchmark::State &state) {
    const char *end = test_data + strlen(test_data);
    for (auto _ : state) {
        const char *iter = test_data;
        while (iter != end) {
            int16_t value = parse_int_table(iter);
            benchmark::DoNotOptimize(value);
        }
    }
}
BENCHMARK(BM_parse_int_table_copy2);

static void BM_noop(benchmark::State &state) {
    for (auto _ : state) {
    }
}
BENCHMARK(BM_noop);

BENCHMARK_MAIN();