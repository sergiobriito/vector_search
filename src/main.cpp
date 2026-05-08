#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "crow.h"
#include "vector_search.hpp"

using namespace std;

int main(int argc, char* argv[]) {
  VectorSearch vector_search;

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

  app.port(9999).concurrency(1).run();

  return 0;
};