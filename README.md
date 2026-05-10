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
- HTTP API using Crow
- optional Nginx load balancer via `docker-compose`

---

## Build and Run Locally

### Build

```bash
make build
```

### Run the API server

```bash
make run
```

This starts the service on port `8080`.

### Health check

```bash
curl http://127.0.0.1:8080/ready
```

Expected response:

```text
ready
```

### Query the fraud API

```bash
curl -X POST http://127.0.0.1:8080/fraud-score \
  -H "Content-Type: application/json" \
  -d @sample-request.json
```

---

## Index Preparation

The service loads `index.bin` from disk when it starts.

To build the index from a JSON reference dataset:

```bash
make build_index
```

This runs `./main build_index` and expects `references.json` to exist in the repository root.
---

## API Reference

### `GET /ready`

Health check endpoint. Returns plain text `ready`.

### `POST /fraud-score`

Processes a transaction request and returns an approval decision and fraud score.

#### Request body

Example request:

```json
{
  "transaction": {
    "amount": 41.12,
    "installments": 2,
    "requested_at": "2026-03-11T18:45:53Z"
  },
  "customer": {
    "avg_amount": 82.24,
    "tx_count_24h": 3,
    "known_merchants": ["MERC-003", "MERC-016"]
  },
  "merchant": {
    "id": "MERC-016",
    "mcc": "5411",
    "avg_amount": 60.25
  },
  "terminal": {
    "is_online": false,
    "card_present": true,
    "km_from_home": 29.2331
  },
  "last_transaction": {
    "timestamp": "2026-03-11T14:58:35Z",
    "km_from_current": 18.8626
  }
}
```

#### Example response

```json
{
  "approved": true,
  "fraud_score": 0.2
}
```

---

## Testing and Benchmarking

The repository includes sample test data at `test-data.json`.

Run the built-in test harness with:

```bash
make run_test
```

This verifies output against expected approvals and fraud scores.

---

## Requirements

- C++17 compiler with AVX2 support
- GNU Make
- Linux environment

