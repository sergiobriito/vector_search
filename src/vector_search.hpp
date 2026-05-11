#ifndef VECTOR_SEARCH_HPP
#define VECTOR_SEARCH_HPP

#include <iostream>
#include <unordered_map>
#include <vector>

#include "ivf.hpp"
#include "utils.hpp"

using namespace std;

class VectorSearch {
 public:
  IVF ivf;
  float threshold = 0.6f;
  size_t dimensions = 16;

  float mcc_risk_array[10000];
  static constexpr int max_amount = 10000;
  static constexpr int max_installments = 12;
  static constexpr int amount_vs_avg_ratio = 10;
  static constexpr int max_minutes = 1440;
  static constexpr int max_km = 1000;
  static constexpr int max_tx_count_24h = 20;
  static constexpr int max_merchant_avg_amount = 10000;

  VectorSearch() { fill_mcc_risk_array(); };

  void load_index() {
    ivf = IVF();
    ivf.load_index_from_binary();
  }

  void fill_mcc_risk_array() {
    fill(begin(mcc_risk_array), end(mcc_risk_array), 0.5f);
    mcc_risk_array[5411] = 0.15f;
    mcc_risk_array[5812] = 0.30f;
    mcc_risk_array[5912] = 0.20f;
    mcc_risk_array[5944] = 0.45f;
    mcc_risk_array[7801] = 0.80f;
    mcc_risk_array[7802] = 0.75f;
    mcc_risk_array[7995] = 0.85f;
    mcc_risk_array[4511] = 0.35f;
    mcc_risk_array[5311] = 0.25f;
    mcc_risk_array[5999] = 0.50f;
  }

  vector<int16_t> transaction_to_vector(const ParsedRequest& req);

  float compute_score(vector<pair<int32_t, uint32_t>>& weights);

  vector<pair<int32_t, uint32_t>> search_neighbors(const vector<int16_t>& vec,
                                                   int top_k);

  pair<bool, float> is_approved(const ParsedRequest& req);

  void create_ivf(const string& filename);
};

#endif
