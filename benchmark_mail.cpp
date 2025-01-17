#include <chrono>

#include <algorithm>
#include <array>
#include <fstream>
#include <ios>
#include <iostream>
#include <iterator>
#include <random>
#include <ranges>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

extern "C" bool match(const char* mail);
extern "C" bool match_ctre(const char* mail);
extern "C" bool match_std(const char* mail);
extern "C" bool match_pcre2(const char* mail);
extern "C" bool match_jit_pcre2(const char* mail);

bool stob(std::string s, bool throw_on_error = true) {
    auto result = false; // failure to assert is false

    std::istringstream is(s);
    // first try simple integer conversion
    is >> result;

    if (is.fail()) {
        // simple integer failed; try boolean
        is.clear();
        is >> std::boolalpha >> result;
    }

    if (is.fail() && throw_on_error) throw std::invalid_argument(s.append(" is not convertable to bool"));

    return result;
}

int main(int argc, const char* argv[]) {
    std::string filename = "annotated.txt";
    if (argc > 1) filename = argv[1];

    std::ifstream ifs{filename};
    if (!ifs) {
        std::cout << "could not read '" << filename << "'\n";
        return 1;
    }

    std::string line;
    std::vector<std::string> emails;
    std::vector<bool> isValid;
    while (std::getline(ifs, line)) {
        const char chr = ',';
        auto pos       = line.find(chr);
        emails.push_back(line.substr(0, pos));
        isValid.push_back(stob(line.substr(pos + 1)));
    }

    std::map<std::string, std::function<void()>> benchs;
    benchs["thorin"] = [&]() {
        for (int i = 0, e = emails.size(); i < e; ++i)
            if (match(emails[i].c_str()) != isValid[i])
                std::cout << "did wrongly (not) match " << emails[i] << "," << std::boolalpha << isValid[i] << "\n";
    };

    benchs["ctre"] = [&]() {
        for (int i = 0, e = emails.size(); i < e; ++i)
            if (match_ctre(emails[i].c_str()) != isValid[i])
                std::cout << "ctre did wrongly (not) match " << emails[i] << "," << std::boolalpha << isValid[i]
                          << "\n";
    };

    benchs["std"] = [&]() {
        for (int i = 0, e = emails.size(); i < e; ++i)
            if (match_std(emails[i].c_str()) != isValid[i])
                std::cout << "std did wrongly (not) match " << emails[i] << "," << std::boolalpha << isValid[i] << "\n";
    };

    benchs["pcre2"] = [&]() {
        for (int i = 0, e = emails.size(); i < e; ++i)
            if (match_pcre2(emails[i].c_str()) != isValid[i])
                std::cout << "pcre2 did wrongly (not) match " << emails[i] << "," << std::boolalpha << isValid[i]
                          << "\n";
    };

    benchs["pcre2_jit"] = [&]() {
        for (int i = 0, e = emails.size(); i < e; ++i)
            if (match_jit_pcre2(emails[i].c_str()) != isValid[i])
                std::cout << "jit pcre2 did wrongly (not) match " << emails[i] << "," << std::boolalpha << isValid[i]
                          << "\n";
    };

    constexpr int N = 10;
    std::map<std::string, std::array<unsigned long, N>> results;
    std::vector<std::string> keys;
    std::transform(benchs.begin(), benchs.end(), std::back_inserter(keys), [](auto kv) { return kv.first; });
    std::random_device rd;
    std::mt19937 g(rd());

    for (int r = 0; r < N; ++r) {
        std::shuffle(keys.begin(), keys.end(), g);
        for (auto& key : keys) {
            // warmup..
            for (int it = 0; it < 100; ++it) benchs[key]();

            // actual measuring
            auto start = std::chrono::high_resolution_clock::now();
            for (int it = 0; it < 100; it++) benchs[key]();
            auto stop       = std::chrono::high_resolution_clock::now();
            results[key][r] = std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count() / 100;
        }
    }

    constexpr char sep = ',';
    std::cout << "engine" << sep << "average[us]" << sep << "min[us]" << sep << "max[us]" << sep << "deviation[%]"
              << sep << "runs[us]\n";
    for (auto& key : keys) {
        auto res      = std::accumulate(results[key].cbegin(), results[key].cend(), 1) / N;
        auto [mi, ma] = std::minmax_element(results[key].cbegin(), results[key].cend());
        std::cout << key << sep << res << sep << *mi << sep << *ma << sep << (100 * (*ma - *mi)) / res << sep;
        for (auto v : results[key]) std::cout << v << " ";
        std::cout << "\n";
    }

    return 0;
}
