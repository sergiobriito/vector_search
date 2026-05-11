#include <simdjson.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string_view>

#include "utils.hpp"
#include "vector_search.hpp"

using namespace std;

void build_index() {
  VectorSearch vector_search;
  vector_search.create_ivf("references.json");
  cout << "k=" << vector_search.ivf.k
       << ", centroids.size()=" << vector_search.ivf.centroids.size()
       << ", bucket_starts.size()=" << vector_search.ivf.bucket_starts.size()
       << endl;
  for (int i = 0; i < min(10, vector_search.ivf.k); i++)
    cout << "bucket " << i << ": "
         << (vector_search.ivf.bucket_starts[i + 1] -
             vector_search.ivf.bucket_starts[i])
         << " vectors" << endl;
}

void run_test() {
  VectorSearch vector_search;
  vector_search.load_index();

  simdjson::ondemand::parser parser;
  simdjson::padded_string json_str;
  if (simdjson::padded_string::load("test-data.json").get(json_str) !=
      simdjson::SUCCESS) {
    return;
  }

  auto doc = parser.iterate(json_str);
  auto entries = doc["entries"].get_array();

  vector<int> times;
  int t = 0, c = 0, n = 0;

  for (auto entry : entries) {
    bool expected_approved = false;
    double expected_fraud_score = 0.0;
    string_view raw_request;

    if (entry["expected_approved"].get_bool().get(expected_approved) !=
        simdjson::SUCCESS)
      break;
    if (entry["expected_fraud_score"].get_double().get(expected_fraud_score) !=
        simdjson::SUCCESS)
      break;
    if (entry["request"].raw_json().get(raw_request) != simdjson::SUCCESS)
      break;

    ParsedRequest parsed = parse_request(raw_request);
    if (!parsed.valid) {
      break;
    }

    auto begin = chrono::high_resolution_clock::now();
    auto [approved, score] = vector_search.is_approved(parsed);
    auto end = chrono::high_resolution_clock::now();
    auto elapsed = chrono::duration_cast<chrono::microseconds>(end - begin);

    bool scores_match = abs(score - expected_fraud_score) < 1e-7;
    bool is_ok = (approved == expected_approved) && scores_match;

    cout << n << " " << (is_ok ? "OK" : "N OK")
         << " | Time: " << elapsed.count() << " microseconds" << endl;

    if (!is_ok) {
      cout << "Real Approved: " << approved
           << " | Expected Approved: " << expected_approved << endl;
      cout << "Real Score: " << score
           << " | Expected Score: " << expected_fraud_score << endl;
      break;
    }

    c += is_ok;
    times.push_back(elapsed.count());
    t += elapsed.count();
    n++;
  }

  if (times.empty()) {
    cout << "No valid results." << endl;
    return;
  }

  sort(times.begin(), times.end());
  int avg = t / (int)times.size();
  int p99 = times[static_cast<size_t>(ceil(0.99 * times.size())) - 1];

  cout << "Result: " << c << "/" << n << endl;
  cout << "AVG: " << avg << " microseconds" << endl;
  cout << "P99: " << p99 << " microseconds" << endl;
}

int main(int argc, char* argv[]) {
  if (argc > 1 && strcmp(argv[1], "build_index") == 0) {
    build_index();
    return 0;
  }

  run_test();

  return 0;
}