#include "utils.hpp"

#include <simdjson.h>

#include <cstring>
#include <string>

static thread_local simdjson::ondemand::parser parser;

ParsedRequest parse_request(string_view body) {
  ParsedRequest result{};

  try {
    simdjson::padded_string padded(body.data(), body.size());
    auto doc = parser.iterate(padded);

    auto tx = doc["transaction"];
    tx["amount"].get_double().get(result.amount);
    std::string_view sv;
    if (tx["requested_at"].get_string().get(sv) == simdjson::SUCCESS)
      result.requested_at = std::string(sv);
    int64_t installments = 0;
    tx["installments"].get_int64().get(installments);
    result.installments = static_cast<int>(installments);

    auto cust = doc["customer"];
    cust["avg_amount"].get_double().get(result.avg_amount);
    int64_t tx_count = 0;
    cust["tx_count_24h"].get_int64().get(tx_count);
    result.tx_count_24h = static_cast<int>(tx_count);
    for (auto m : cust["known_merchants"]) {
      std::string_view s;
      if (m.get_string().get(s) == simdjson::SUCCESS)
        result.known_merchants.emplace_back(s);
    }

    auto merch = doc["merchant"];
    if (merch["id"].get_string().get(sv) == simdjson::SUCCESS)
      result.merchant_id = std::string(sv);
    if (merch["mcc"].get_string().get(sv) == simdjson::SUCCESS)
      result.mcc_str = std::string(sv);
    merch["avg_amount"].get_double().get(result.merchant_avg);

    auto term = doc["terminal"];
    term["is_online"].get_bool().get(result.is_online);
    term["card_present"].get_bool().get(result.card_present);
    term["km_from_home"].get_double().get(result.km_from_home);

    auto last = doc["last_transaction"];
    bool is_null = false;
    if (last.is_null().get(is_null) == simdjson::SUCCESS && !is_null) {
      result.has_last_tx = true;
      if (last["timestamp"].get_string().get(sv) == simdjson::SUCCESS)
        result.timestamp = std::string(sv);
      last["km_from_current"].get_double().get(result.km_from_current);
    }

    result.valid = true;
  } catch (...) {
    result.valid = false;
  }

  return result;
}

ParsedDateTime parse_datetime(const string& s) {
  ParsedDateTime result;

  int year = (s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[2] - '0') * 10 +
             (s[3] - '0');
  int mon = (s[5] - '0') * 10 + (s[6] - '0');
  int day = (s[8] - '0') * 10 + (s[9] - '0');
  int hour = (s[11] - '0') * 10 + (s[12] - '0');
  int min = (s[14] - '0') * 10 + (s[15] - '0');
  int sec = (s[17] - '0') * 10 + (s[18] - '0');

  result.tm.tm_year = year - 1900;
  result.tm.tm_mon = mon - 1;
  result.tm.tm_mday = day;
  result.tm.tm_hour = hour;
  result.tm.tm_min = min;
  result.tm.tm_sec = sec;
  result.tm.tm_isdst = -1;

#ifdef _WIN32
  result.unix_time = _mkgmtime(&result.tm);
#else
  result.unix_time = timegm(&result.tm);
#endif

  return result;
}