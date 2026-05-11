#include "utils.hpp"

#include <cstring>
#include <nlohmann/json.hpp>
#include <string>

ParsedRequest parse_request(string_view body) {
  ParsedRequest result{};
  result.valid = true;

  try {
    nlohmann::json j = nlohmann::json::parse(body);

    if (j.contains("transaction")) {
      auto& tx = j["transaction"];
      if (tx.contains("amount")) result.amount = tx["amount"].get<double>();
      if (tx.contains("installments"))
        result.installments = tx["installments"].get<int>();
      if (tx.contains("requested_at"))
        result.requested_at = tx["requested_at"].get<string>();
    }

    if (j.contains("customer")) {
      auto& cust = j["customer"];
      if (cust.contains("avg_amount"))
        result.avg_amount = cust["avg_amount"].get<double>();
      if (cust.contains("tx_count_24h"))
        result.tx_count_24h = cust["tx_count_24h"].get<int>();
      if (cust.contains("known_merchants")) {
        for (auto& m : cust["known_merchants"])
          result.known_merchants.push_back(m.get<string>());
      }
    }

    if (j.contains("merchant")) {
      auto& merch = j["merchant"];
      if (merch.contains("id"))
        result.merchant_id = merch["id"].get<string>();
      if (merch.contains("mcc"))
        result.mcc_str = merch["mcc"].get<string>();
      if (merch.contains("avg_amount"))
        result.merchant_avg = merch["avg_amount"].get<double>();
    }

    if (j.contains("terminal")) {
      auto& term = j["terminal"];
      if (term.contains("is_online"))
        result.is_online = term["is_online"].get<bool>();
      if (term.contains("card_present"))
        result.card_present = term["card_present"].get<bool>();
      if (term.contains("km_from_home"))
        result.km_from_home = term["km_from_home"].get<double>();
    }

    if (j.contains("last_transaction") && !j["last_transaction"].is_null()) {
      auto& last = j["last_transaction"];
      result.has_last_tx = true;
      if (last.contains("timestamp"))
        result.timestamp = last["timestamp"].get<string>();
      if (last.contains("km_from_current"))
        result.km_from_current = last["km_from_current"].get<double>();
    }

  } catch (const exception& e) {
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