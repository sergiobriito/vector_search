#include "vector_search.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <unordered_map>
#include <vector>

struct ParsedDateTime {
  time_t unix_time;
  struct tm tm;
};

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

vector<int16_t> VectorSearch::transaction_to_vector(
    const crow::json::rvalue& j) {
  vector<int16_t> vec(dimensions, 0);
  constexpr float scale = 10000.0f;

  constexpr float inv_max_amount = 1.0f / max_amount;
  constexpr float inv_max_installments = 1.0f / max_installments;
  constexpr float inv_amount_vs_avg_ratio = 1.0f / amount_vs_avg_ratio;
  constexpr float inv_max_hour = 1.0f / 23.0f;
  constexpr float inv_max_wday = 1.0f / 6.0f;
  constexpr float inv_max_minutes = 1.0f / max_minutes;
  constexpr float inv_max_km = 1.0f / max_km;
  constexpr float inv_max_tx_count = 1.0f / max_tx_count_24h;
  constexpr float inv_max_merchant_avg = 1.0f / max_merchant_avg_amount;

  auto quantize = [](float val) -> int16_t {
    val = clamp(val, 0.0f, 1.0f);
    return (int16_t)lrintf(val * scale);
  };

  float amount = j["transaction"]["amount"].d();
  float norm_amount = amount * inv_max_amount;
  vec[0] = quantize(norm_amount);

  int installments = j["transaction"]["installments"].d();
  float norm_inst = installments * inv_max_installments;
  vec[1] = quantize(norm_inst);

  float avg_amount = j["customer"]["avg_amount"].d();
  float ratio = amount / avg_amount;
  float norm_ratio = ratio * inv_amount_vs_avg_ratio;
  vec[2] = quantize(norm_ratio);

  const string& ts_str = j["transaction"]["requested_at"].s();

  ParsedDateTime dt = parse_datetime(ts_str);
  int hour = dt.tm.tm_hour;
  float norm_hour = hour * inv_max_hour;
  vec[3] = quantize(norm_hour);

  int wday = (dt.tm.tm_wday + 6) % 7;
  float norm_wday = wday * inv_max_wday;
  vec[4] = quantize(norm_wday);

  time_t unix_time = dt.unix_time;

  if (j.has("last_transaction") &&
      j["last_transaction"].t() == crow::json::type::Object) {
    auto last_tx = j["last_transaction"];
    if (last_tx.has("timestamp") && last_tx.has("km_from_current")) {
      string last_ts = last_tx["timestamp"].s();
      ParsedDateTime last_dt = parse_datetime(last_ts);
      float minutes = (unix_time - last_dt.unix_time) / 60.0f;
      vec[5] = quantize(minutes * inv_max_minutes);

      float km_last = last_tx["km_from_current"].d();
      vec[6] = quantize(km_last * inv_max_km);
    } else {
      vec[5] = -10000;
      vec[6] = -10000;
    }
  } else {
    vec[5] = -10000;
    vec[6] = -10000;
  }

  float km_home = j["terminal"]["km_from_home"].d();
  vec[7] = quantize(km_home * inv_max_km);

  int tx_count = j["customer"]["tx_count_24h"].d();
  vec[8] = quantize(tx_count * inv_max_tx_count);

  vec[9] = quantize(j["terminal"]["is_online"].b() ? 1.0f : 0.0f);

  vec[10] = quantize(j["terminal"]["card_present"].b() ? 1.0f : 0.0f);

  string merchant_id = j["merchant"]["id"].s();
  const auto& known = j["customer"]["known_merchants"];
  bool is_unknown =
      find(known.begin(), known.end(), merchant_id) == known.end();
  vec[11] = quantize(is_unknown ? 1.0f : 0.0f);

  string mcc_str = j["merchant"]["mcc"].s();
  int mcc_int = 0;
  for (char c : mcc_str) {
    if (c >= '0' && c <= '9')
      mcc_int = mcc_int * 10 + (c - '0');
    else
      break;
  }
  if (mcc_int >= 0 && mcc_int < 10000) {
    vec[12] = quantize(mcc_risk_array[mcc_int]);
  } else {
    vec[12] = quantize(0.5f);
  }

  float merchant_avg = j["merchant"]["avg_amount"].d();
  vec[13] = quantize(merchant_avg * inv_max_merchant_avg);

  return vec;
}

float VectorSearch::compute_score(vector<pair<int32_t, uint32_t>>& weights) {
  int t = 0;
  for (auto& [dist, is_fraud] : weights) {
    if (is_fraud) t++;
  }
  return static_cast<float>(t) / weights.size();
}

vector<pair<int32_t, uint32_t>> VectorSearch::search_neighbors(
    const vector<int16_t>& vec, int top_k) {
  return ivf.search(vec, top_k);
}

pair<bool, float> VectorSearch::is_approved(const crow::json::rvalue& j) {
  vector<int16_t> vec = transaction_to_vector(j);
  vector<pair<int32_t, uint32_t>> weights = search_neighbors(vec, 5);
  float score = compute_score(weights);
  return {score < threshold, score};
}

void VectorSearch::create_ivf(const string& filename) {
  ifstream file(filename);
  if (!file.is_open()) {
    return;
  }

  nlohmann::json j = nlohmann::json::parse(file);

  size_t total_vectors = j.size();
  vector<int16_t> all_vectors(total_vectors * dimensions);
  vector<uint8_t> labels(total_vectors);
  vector<uint32_t> vector_ids(total_vectors);

  for (size_t i = 0; i < total_vectors; i++) {
    const auto& vec = j[i]["vector"];
    for (size_t d = 0; d < dimensions; d++) {
      if (d < 14) {
        float val = vec[d].get<float>();
        if (val == -1.0f) {
          all_vectors[i * dimensions + d] = -10000;
        } else {
          val = (val < 0.0f) ? 0.0f : (val > 1.0f ? 1.0f : val);
          all_vectors[i * dimensions + d] =
              static_cast<int16_t>(lrintf(val * 10000.0f));
        }
      } else {
        all_vectors[i * dimensions + d] = 0;
      }
    }
    labels[i] = (j[i]["label"] == "fraud") ? 1 : 0;
    vector_ids[i] = (uint32_t)i;
  }

  ivf.build_index(all_vectors, labels, vector_ids);
  ivf.save_index_to_binary();
}
