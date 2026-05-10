#ifndef IVF_HPP
#define IVF_HPP

#include <cstdint>
#include <iostream>
#include <vector>

using namespace std;

class IVF {
 public:
  vector<int16_t> all_vectors;
  vector<uint8_t> labels;
  vector<uint32_t> vector_ids;
  vector<int16_t> centroids;
  vector<int> bucket_starts;

  int k = 1024;
  int sample_size = 500000;
  int max_iters = 100;
  int nprobe = 1;

  string index_filename = "index.bin";

  IVF() {
    centroids.resize(k * 16);
    bucket_starts.resize(k + 1, 0);
  }

  vector<pair<int32_t, uint32_t>> search(const vector<int16_t>& query,
                                         int top_k);

  void build_index(const vector<int16_t>& data, const vector<uint8_t>& lbls,
                   const vector<uint32_t>& ids);

  void k_means(const vector<int16_t>& data);

  int32_t euclidean_distance(const int16_t* a, const int16_t* b);

  void save_index_to_binary();

  void load_index_from_binary();
};

#endif
