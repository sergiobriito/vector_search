#include <sys/stat.h>
#include <uWebSockets/App.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include <string_view>

#include "utils.hpp"
#include "vector_search.hpp"

using namespace std;

static const char* responses[6] = {"{\"approved\":true,\"fraud_score\":0.0}",
                                   "{\"approved\":true,\"fraud_score\":0.2}",
                                   "{\"approved\":true,\"fraud_score\":0.4}",
                                   "{\"approved\":false,\"fraud_score\":0.6}",
                                   "{\"approved\":false,\"fraud_score\":0.8}",
                                   "{\"approved\":false,\"fraud_score\":1.0}"};

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

  ifstream file("test-data.json");
  if (!file) {
    return;
  }

  nlohmann::json data;
  file >> data;

  auto entries = data["entries"];
  vector<int> times;

  int t = 0;
  int c = 0;
  int n = entries.size();

  for (int i = 0; i < n; i++) {
    nlohmann::json request_json = entries[i]["request"];
    string request_str = request_json.dump();

    bool expected_approved = entries[i]["expected_approved"];
    double expected_fraud_score = entries[i]["expected_fraud_score"];

    ParsedRequest parsed = parse_request(request_str);
    if (!parsed.valid) {
      break;
    }

    auto begin = chrono::high_resolution_clock::now();
    auto [approved, score] = vector_search.is_approved(parsed);
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
}

void warmup(VectorSearch& vector_search) {
  string j = R"({
        "id": "dummy",
        "transaction": {"amount": 100.0, "installments": 1, "requested_at": "2026-05-10T12:00:00Z"},
        "customer": {"avg_amount": 100.0, "tx_count_24h": 1, "known_merchants": []},
        "merchant": {"id": "M-1", "mcc": "0000", "avg_amount": 100.0},
        "terminal": {"is_online": true, "card_present": true, "km_from_home": 0.0},
        "last_transaction": {"timestamp": "2026-05-10T11:00:00Z", "km_from_current": 0.0}
    })";

  ParsedRequest parsed = parse_request(j);
  if (!parsed.valid) {
    return;
  }

  for (int i = 0; i < 1000; i++) {
    vector_search.is_approved(parsed);
  }
}

int main(int argc, char* argv[]) {
  if (argc > 1 && strcmp(argv[1], "build_index") == 0) {
    build_index();
    return 0;
  }

  if (argc > 1 && strcmp(argv[1], "run_test") == 0) {
    run_test();
    return 0;
  }

  VectorSearch vector_search;
  vector_search.load_index();
  warmup(vector_search);

  uWS::App app;

  app.get("/ready", [](auto* res, auto* req) { res->end("ready"); });

  app.post("/fraud-score", [&vector_search](auto* res, auto* req) {
    res->onData([res, &vector_search](string_view chunk, bool isLast) {
      if (isLast) {
        ParsedRequest parsed = parse_request(chunk);
        auto [approved, score] = vector_search.is_approved(parsed);
        res->writeHeader("Content-Type", "application/json")
            ->end(responses[static_cast<int>(score * 5 + 0.5)]);
      }
    });
  });

  const char* socket_path = getenv("SOCKET_PATH");

  if (!socket_path) {
    socket_path = "/sockets/api.sock";
  }

  unlink(socket_path);

  app.listen(socket_path, 0,
             [socket_path](auto* listen_socket) {
               if (listen_socket) {
                 chmod(socket_path, 0777);
                 cout << "Listening: " << socket_path << endl;
               } else {
                 cout << "Failed: " << socket_path << endl;
               }
             })
      .run();

  return 0;
}