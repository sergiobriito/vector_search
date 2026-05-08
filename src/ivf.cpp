#include "ivf.hpp"

#include <immintrin.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <limits>
#include <queue>
#include <random>
#include <sstream>
#include <vector>

using namespace std;

vector<pair<float, bool>> IVF::search(const vector<int16_t>& query, int top_k,
                                      int nprobe) {
  const int16_t* q_ptr = query.data();
  __m256i v_query = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(q_ptr));

  priority_queue<pair<int32_t, int>> closest_centroids;

  for (int c = 0; c < k; c++) {
    __m256i v_cent = _mm256_loadu_si256(
        reinterpret_cast<const __m256i*>(&centroids[c * 16]));
    __m256i diff = _mm256_sub_epi16(v_query, v_cent);
    __m256i dot = _mm256_madd_epi16(diff, diff);
    __m128i hi = _mm256_extracti128_si256(dot, 1);
    __m128i lo = _mm256_castsi256_si128(dot);
    __m128i sum = _mm_add_epi32(hi, lo);
    sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, _MM_SHUFFLE(1, 0, 3, 2)));
    sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, _MM_SHUFFLE(2, 3, 0, 1)));
    int32_t d = _mm_cvtsi128_si32(sum);

    if (closest_centroids.size() < (size_t)nprobe) {
      closest_centroids.push({d, c});
    } else if (d < closest_centroids.top().first) {
      closest_centroids.pop();
      closest_centroids.push({d, c});
    }
  }

  vector<pair<int32_t, bool>> top_results;

  auto update_topk = [&](int32_t dist, bool label) {
    if (top_results.size() < (size_t)top_k) {
      top_results.push_back({dist, label});
      push_heap(top_results.begin(), top_results.end());
    } else if (dist < top_results.front().first) {
      pop_heap(top_results.begin(), top_results.end());
      top_results.back() = {dist, label};
      push_heap(top_results.begin(), top_results.end());
    }
  };

  while (!closest_centroids.empty()) {
    int best_c = closest_centroids.top().second;
    closest_centroids.pop();

    int start = bucket_starts[best_c];
    int end = bucket_starts[best_c + 1];

    int j = start;
    for (; j + 7 < end; j += 8) {
      _mm_prefetch(reinterpret_cast<const char*>(&all_vectors[(j + 8) * 16]),
                   _MM_HINT_T0);

      auto get_dist = [&](int offset) -> int32_t {
        __m256i v = _mm256_loadu_si256(
            reinterpret_cast<const __m256i*>(&all_vectors[(j + offset) * 16]));
        __m256i diff = _mm256_sub_epi16(v_query, v);
        __m256i dot = _mm256_madd_epi16(diff, diff);
        __m128i hi = _mm256_extracti128_si256(dot, 1);
        __m128i lo = _mm256_castsi256_si128(dot);
        __m128i sum = _mm_add_epi32(hi, lo);
        sum =
            _mm_add_epi32(sum, _mm_shuffle_epi32(sum, _MM_SHUFFLE(1, 0, 3, 2)));
        sum =
            _mm_add_epi32(sum, _mm_shuffle_epi32(sum, _MM_SHUFFLE(2, 3, 0, 1)));
        return _mm_cvtsi128_si32(sum);
      };

      for (int i = 0; i < 8; ++i) update_topk(get_dist(i), labels[j + i]);
    }

    for (; j < end; ++j) {
      const int16_t* vec = &all_vectors[j * 16];
      int32_t dist_sq = euclidean_distance(q_ptr, vec);
      update_topk(dist_sq, labels[j]);
    }
  }

  sort_heap(top_results.begin(), top_results.end());
  vector<pair<float, bool>> res;
  for (auto& p : top_results) {
    res.emplace_back(static_cast<float>(p.first) * 1e-8f, p.second);
  }
  return res;
}

void IVF::build_index(const vector<int16_t>& data, const vector<uint8_t>& lbls,
                      const vector<uint32_t>& ids) {
  size_t n = lbls.size();
  k_means(data);

  vector<vector<size_t>> buckets(k);
  for (size_t i = 0; i < n; i++) {
    float min_dist = 1e30f;
    int best_cluster = 0;
    const int16_t* v_ptr = &data[i * 16];
    for (int c = 0; c < k; c++) {
      int32_t d = euclidean_distance(v_ptr, &centroids[c * 16]);
      if (d < min_dist) {
        min_dist = d;
        best_cluster = c;
      }
    }
    buckets[best_cluster].push_back(i);
  }

  all_vectors.assign(n * 16, 0);
  labels.assign(n, 0);
  vector_ids.assign(n, 0);
  bucket_starts.assign(k + 1, 0);

  int current = 0;
  for (int c = 0; c < k; c++) {
    bucket_starts[c] = current;
    for (size_t idx : buckets[c]) {
      for (size_t d = 0; d < 16; d++)
        all_vectors[current * 16 + d] = data[idx * 16 + d];
      labels[current] = lbls[idx];
      vector_ids[current] = ids[idx];
      current++;
    }
  }
  bucket_starts[k] = current;
}

void IVF::k_means(const vector<int16_t>& data) {
  size_t n_total = data.size() / 16;
  size_t train_stride = std::max<size_t>(1, n_total / sample_size);

  vector<size_t> sample_indices;
  for (int i = 0; i < sample_size && (i * train_stride) < n_total; ++i) {
    sample_indices.push_back(i * train_stride);
  }
  size_t n_sample = sample_indices.size();

  static mt19937_64 rng(time(nullptr));
  vector<bool> used(n_sample, false);
  vector<int64_t> min_dist2(n_sample, numeric_limits<int64_t>::max());

  uniform_int_distribution<size_t> dist(0, n_sample - 1);
  size_t first = dist(rng);
  used[first] = true;
  size_t idx0 = sample_indices[first];
  for (size_t d = 0; d < 16; ++d) centroids[0 * 16 + d] = data[idx0 * 16 + d];

  for (int c = 1; c < k; ++c) {
#pragma omp parallel for
    for (size_t i = 0; i < n_sample; ++i) {
      if (used[i]) continue;
      size_t idx = sample_indices[i];
      int32_t d2 =
          euclidean_distance(&data[idx * 16], &centroids[(c - 1) * 16]);
      if (d2 < min_dist2[i]) min_dist2[i] = d2;
    }
    int64_t total = 0;
    for (size_t i = 0; i < n_sample; ++i) {
      if (!used[i]) total += min_dist2[i];
    }
    if (total == 0) {
      vector<size_t> candidates;
      for (size_t i = 0; i < n_sample; ++i)
        if (!used[i]) candidates.push_back(i);
      if (candidates.empty()) break;
      uniform_int_distribution<size_t> pick(0, candidates.size() - 1);
      size_t chosen = candidates[pick(rng)];
      used[chosen] = true;
      size_t idx = sample_indices[chosen];
      for (size_t d = 0; d < 16; ++d)
        centroids[c * 16 + d] = data[idx * 16 + d];
      continue;
    }

    uniform_int_distribution<int64_t> dist(0, total - 1);
    int64_t r = dist(rng);
    int64_t cum = 0;
    size_t chosen = 0;
    for (size_t i = 0; i < n_sample; ++i) {
      if (used[i]) continue;
      cum += min_dist2[i];
      if (cum > r) {
        chosen = i;
        break;
      }
    }
    used[chosen] = true;
    size_t idx = sample_indices[chosen];
    for (size_t d = 0; d < 16; ++d) centroids[c * 16 + d] = data[idx * 16 + d];
  }

  for (int iter = 0; iter < max_iters; iter++) {
    vector<vector<float>> acc(k, vector<float>(16, 0.0f));
    vector<int> counts(k, 0);

    for (int i = 0; i < sample_size && (i * train_stride) < n_total; ++i) {
      const int16_t* v = &data[(i * train_stride) * 16];
      int best_c = 0;
      float min_d = numeric_limits<float>::max();
      for (int c = 0; c < k; ++c) {
        int32_t d = euclidean_distance(v, &centroids[c * 16]);
        if (d < min_d) {
          min_d = d;
          best_c = c;
        }
      }
      for (size_t d = 0; d < 16; ++d) acc[best_c][d] += v[d];
      counts[best_c]++;
    }

    for (int c = 0; c < k; ++c) {
      if (counts[c] > 0) {
        for (size_t d = 0; d < 16; ++d) {
          centroids[c * 16 + d] =
              static_cast<int16_t>(std::round(acc[c][d] / counts[c]));
        }
      }
    }
  }
}

int32_t IVF::euclidean_distance(const int16_t* a, const int16_t* b) {
  __m256i v_a = _mm256_loadu_si256((const __m256i*)a);
  __m256i v_b = _mm256_loadu_si256((const __m256i*)b);
  __m256i diff = _mm256_sub_epi16(v_a, v_b);
  __m256i dot = _mm256_madd_epi16(diff, diff);
  __m128i hi = _mm256_extracti128_si256(dot, 1);
  __m128i lo = _mm256_castsi256_si128(dot);
  __m128i sum = _mm_add_epi32(hi, lo);
  sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, _MM_SHUFFLE(1, 0, 3, 2)));
  sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, _MM_SHUFFLE(2, 3, 0, 1)));
  return _mm_cvtsi128_si32(sum);
}

void IVF::save_index_to_binary() {
  ofstream file(index_filename, ios::binary);
  if (!file) return;
  size_t s_all = all_vectors.size(), s_labels = labels.size(),
         s_ids = vector_ids.size(), s_centroids = centroids.size(),
         s_buckets = bucket_starts.size();
  file.write((char*)&s_all, sizeof(size_t));
  file.write((char*)&s_labels, sizeof(size_t));
  file.write((char*)&s_ids, sizeof(size_t));
  file.write((char*)&s_centroids, sizeof(size_t));
  file.write((char*)&s_buckets, sizeof(size_t));
  file.write((char*)all_vectors.data(), s_all * 2);
  file.write((char*)labels.data(), s_labels);
  file.write((char*)vector_ids.data(), s_ids * 4);
  file.write((char*)centroids.data(), s_centroids * 2);
  file.write((char*)bucket_starts.data(), s_buckets * 4);
}

void IVF::load_index_from_binary() {
  ifstream file(index_filename, ios::binary);
  if (!file) return;
  size_t s_all, s_labels, s_ids, s_centroids, s_buckets;
  file.read((char*)&s_all, sizeof(size_t));
  file.read((char*)&s_labels, sizeof(size_t));
  file.read((char*)&s_ids, sizeof(size_t));
  file.read((char*)&s_centroids, sizeof(size_t));
  file.read((char*)&s_buckets, sizeof(size_t));
  all_vectors.resize(s_all);
  file.read((char*)all_vectors.data(), s_all * 2);
  labels.resize(s_labels);
  file.read((char*)labels.data(), s_labels);
  vector_ids.resize(s_ids);
  file.read((char*)vector_ids.data(), s_ids * 4);
  centroids.resize(s_centroids);
  file.read((char*)centroids.data(), s_centroids * 2);
  bucket_starts.resize(s_buckets);
  file.read((char*)bucket_starts.data(), s_buckets * 4);
}