# IVF-Based Vector Search

This project implements a fraud scoring service using an **Inverted File Index (IVF)** with 16‑dimensional quantized vectors.  
It receives a transaction JSON via HTTP, transforms it into a feature vector, searches the nearest neighbours in the pre‑built index, and returns an approval decision along with a fraud score.

## Index Preparation

Before starting the server, you must build the IVF index from a training dataset.  
The index builder expects a JSON file with the following format:

```
[
  {
    "vector": [0.123, 0.456, ... , 0.789],   // 16 elements, each in [0,1] or -1.0 for missing
    "label": "fraud" | "legit"
  },
  ...
]
```

To create the index, call `VectorSearch::create_ivf("references.json")` once.  
The resulting binary index (`index.bin`) will be loaded automatically on server startup.

## Running the Server

```bash
./fraud_server
```

The server listens on `http://0.0.0.0:9999`.

### Endpoints

| Method | Path           | Description                          |
|--------|----------------|--------------------------------------|
| GET    | `/ready`       | Returns `"ready"` (health check)     |
| POST   | `/fraud-score` | Accepts transaction JSON, returns approval decision and fraud score. |

## API Usage

### Request (`/fraud-score`)

**Required fields**:

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

### Response

```json
{
  "approved": true,
  "fraud_score": 0.2
}
```
