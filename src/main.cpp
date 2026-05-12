#include <simdjson.h>
#include <sys/stat.h>
#include <uWebSockets/App.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
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

void warmup(VectorSearch& vector_search) {
  string j = R"({
        "id": "dummy",
        "transaction": {"amount": 100.0, "installments": 1, "requested_at": "2026-05-10T12:00:00Z"},
        "customer": {"avg_amount": 100.0, "tx_count_24h": 1, "known_merchants": []},
        "merchant": {"id": "M-1", "mcc": "0000", "avg_amount": 100.0},
        "terminal": {"is_online": true, "card_present": true, "km_from_home": 0.0},
        "last_transaction": {"timestamp": "2026-05-10T11:00:00Z", "km_from_current": 0.0}
    })";

  j.append(simdjson::SIMDJSON_PADDING, '\0');
  string_view sv(j.data(), j.size() - simdjson::SIMDJSON_PADDING);

  ParsedRequest parsed = parse_request(sv);
  if (!parsed.valid) return;

  for (int i = 0; i < 5000; i++) {
    vector_search.is_approved(parsed);
  }
}

int main(int argc, char* argv[]) {
  VectorSearch vector_search;
  vector_search.load_index();
  warmup(vector_search);

  uWS::App app;

  app.get("/ready", [](auto* res, auto* req) { res->end("ready"); });

  app.post("/fraud-score", [&vector_search](auto* res, auto* req) {
    auto body = make_shared<string>();
    body->reserve(1024);

    auto aborted = false;

    res->onAborted([&aborted]() { aborted = true; });

    res->onData(
        [res, &vector_search, body, &aborted](string_view chunk, bool isLast) {
          body->append(chunk.data(), chunk.size());
          if (isLast && !aborted) {
            ParsedRequest parsed = parse_request(*body);
            auto [approved, score] = vector_search.is_approved(parsed);

            int index = max(0, min(5, static_cast<int>(score * 5 + 0.5)));
            res->end(responses[index]);
          }
        });
  });

  const char* socket_path = getenv("SOCKET_PATH");
  if (!socket_path) {
    socket_path = "/sockets/api.sock";
  }

  unlink(socket_path);

  mode_t old_mask = umask(0);

  app.listen(
      [&](auto* listen_socket) {
        if (listen_socket) {
          cout << "Listening: " << socket_path << endl;
        } else {
          cerr << "Failed: " << socket_path << endl;
          exit(1);
        }
      },
      socket_path);

  chmod(socket_path, 0777);

  umask(old_mask);
  app.run();

  return 0;
}