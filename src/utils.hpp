#ifndef UTILS_HPP
#define UTILS_HPP

#include <cstdint>
#include <iostream>
#include <vector>

using namespace std;

struct ParsedRequest {
  double amount;
  double avg_amount;
  double km_from_home;
  double merchant_avg;
  double km_from_current;
  int installments;
  int tx_count_24h;
  bool is_online;
  bool card_present;
  string merchant_id;
  string mcc_str;
  string requested_at;
  string timestamp;
  vector<string> known_merchants;
  bool has_last_tx;
  bool valid;
};

struct ParsedDateTime {
  time_t unix_time;
  struct tm tm;
};

ParsedRequest parse_request(string_view body);

ParsedDateTime parse_datetime(const string& s);

#endif
