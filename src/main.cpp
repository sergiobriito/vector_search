#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "crow.h"
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
};

void run_test() {
  VectorSearch vector_search;
  vector_search.load_index();

  ifstream file("test-data.json");
  if (!file) {
    return;
  };

  stringstream buffer;
  buffer << file.rdbuf();
  string json_str = buffer.str();

  auto data = crow::json::load(json_str);
  auto entries = data["entries"];
  vector<int> times;

  // Warmup
  for (int i = 0; i < 1000; i++) {
    vector_search.is_approved(entries[i % entries.size()]["request"]);
  }

  int t = 0;
  int c = 0;
  int n = entries.size();
  for (int i = 0; i < n; i++) {
    auto request = entries[i]["request"];
    auto expected_approved = entries[i]["expected_approved"].b();
    auto expected_fraud_score = entries[i]["expected_fraud_score"].d();

    auto begin = chrono::high_resolution_clock::now();
    auto [approved, score] = vector_search.is_approved(request);
    auto end = chrono::high_resolution_clock::now();
    auto elapsed = chrono::duration_cast<chrono::microseconds>(end - begin);

    bool scores_match = abs(score - expected_fraud_score) < 1e-7;
    bool is_ok = (approved == expected_approved) && scores_match;

    cout << i << " " << (is_ok ? "OK" : "N OK")
         << " | Time: " << elapsed.count() << " microseconds" << endl;

    if (!is_ok) {
      cout << "Real Approved: " << approved
           << " | Expected Approved: " << expected_approved << endl;
      cout << "Real Score: " << score
           << " | Expected Score: " << expected_fraud_score << endl;
      break;
    };

    c += is_ok;
    times.push_back(elapsed.count());
    t += elapsed.count();
  };

  sort(times.begin(), times.end());
  int avg = t / times.size();
  int p99 = times[static_cast<size_t>(ceil(0.99 * times.size())) - 1];

  cout << "Result: " << c << "/" << n << endl;
  cout << "AVG: " << avg << " microseconds" << endl;
  cout << "P99: " << p99 << " microseconds" << endl;
};

int main(int argc, char* argv[]) {
  if (argc > 1 && strcmp(argv[1], "build_index") == 0) {
    build_index();
    return 0;
  };

  if (argc > 1 && strcmp(argv[1], "run_test") == 0) {
    run_test();
    return 0;
  };

  VectorSearch vector_search;
  vector_search.load_index();

  crow::SimpleApp app;

  CROW_ROUTE(app, "/ready")([]() { return "ready"; });

  CROW_ROUTE(app, "/fraud-score")
      .methods(crow::HTTPMethod::POST)(
          [&vector_search](const crow::request& req) {
            auto body = crow::json::load(req.body);
            if (!body) return crow::response(400, "Invalid JSON");
            auto [approved, score] = vector_search.is_approved(body);
            crow::json::wvalue response;
            response["approved"] = approved;
            response["fraud_score"] = score;
            return crow::response{response};
          });

  app.port(8080).multithreaded().run();

  return 0;
};