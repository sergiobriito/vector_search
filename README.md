## Vector Search for Fraud Scoring

This study project explores high-performance vector search optimized for fraud scoring within limited resources.
The challenge involved implementing an **Inverted File Index (IVF)** capable of running under 1 CPU and 350MB of RAM for all services.

### Optimizations

* Utilized **int16 quantization** and container tuning to stay within the 350MB limit.
* Leveraged **SIMD** and custom JSON parsers to reduce overhead.
* Transformed 14 dimension vectors into 16 dimension vectors to ensure proper padding alignment for faster processing.
* Implemented an optimized K-Means for KNN and a Euclidean distance calculation.

### Training

The IVF index was trained on 3 million vectors, with parameter tuning to find the optimal balance between latency and accuracy. 