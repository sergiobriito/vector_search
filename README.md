# Vector Search Fraud Scoring Service

A fraud scoring service built on a Inverted File Index (IVF).

The service:
- converts transaction JSON into a quantized 16-dimensional feature vector
- searches the nearest neighbors in a pre-built IVF index
- computes a fraud score from the neighbor labels
- returns a approval decision and a fraud score

---

## Features

- IVF index with k-means centroid clustering
- SIMD-accelerated distance search with AVX2
- HTTP API and HAProxy load balancer via `docker-compose`