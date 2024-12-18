
# Entropy Collector

This C program collects system entropy by using various system data sources and produces a seed for cryptographic use, utilizing OpenSSL's SHA-256 for hashing. It includes a fallback to /dev/urandom if primary sources fail.


## Features

Entropy Sources:

- Clock jitter (CLOCK_MONOTONIC and CLOCK_MONOTONIC_RAW): Variability in clock timing.
- CPU statistics from /proc/stat: Data about CPU usage and performance metrics.
- System temperature from /sys/class/thermal/thermal_zone0/temp: Temperature readings to introduce variability in the entropy pool.
- System load averages from /proc/loadavg: Measurement of the computer's temperature for thermal monitoring.

Safety measures:
 - Fallback: If primary entropy sources are unavailable, it falls back to /dev/urandom.
 - SHA-256 Hashing: The collected entropy is hashed using SHA-256 to provide a secure seed for cryptographic applications.


## Usage

This project was developed as a learning exercise to explore Random Number Generators (RNGs) and Cryptographically Secure Pseudo-Random Number Generators (CSPRNGs). **Please note that this implementation has not been rigorously tested for cryptographic security! It is not suitable for real-world or production use.**

![entropyval1](https://github.com/user-attachments/assets/8fcb6ea1-129a-48ab-86f2-c79bc63c3db2)


The entropy sources gathered here might introduce sufficient randomness to produce unpredictable pseudo-random output; however, this is not guaranteed. As seen in the graph, very similar values sometimes repeat in close succession. This lack of variation could be due to minimal changes between ticks, as entropy is collected during each loop iteration in the **collect_entropy()** function.
