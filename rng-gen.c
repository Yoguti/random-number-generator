#define _POSIX_C_SOURCE 199309L  // Enable POSIX.1b features for clock_gettime
#include <stdio.h>               
#include <stdlib.h>              
#include <stdint.h>              
#include <string.h>              
#include <time.h>                // For clock_gettime and timespec structure

#include <openssl/sha.h>
// Base entropy generator
// Uses temperature sensor, clock jitter, CPU process stats, network packages.

struct {
    uint64_t user_time;
    uint64_t system_time;
    uint64_t idle_time;
} entropy = {0}; //safety

static int failure = 0;

int get_failure() {
    return failure;
}

void set_failure(int value) {
    failure = value;
}

// Fallback function to use /dev/urandom
void fallback() {
    if (get_failure() == 0) { // Set failure only once to prevent overwriting
        set_failure(1);
    }
    FILE *urandom = fopen("/dev/urandom", "r");
    if (urandom) {
        if (fread(&entropy, sizeof(entropy), 1, urandom) != 1) {
            perror("Error reading /dev/urandom");
        }
        fclose(urandom);
    } else {
        perror("Error opening /dev/urandom");
    }
}

// CPU noise collection from /proc/stat
void cpu_noise() {
    if (get_failure() == 0) {
        FILE *fp = fopen("/proc/stat", "r");
        if (fp == NULL) {
            fallback(); 
            perror("Error opening /proc/stat");
            return;
        }

        char buffer[256];
        while (fgets(buffer, sizeof(buffer), fp) != NULL) {
            if (strncmp(buffer, "cpu", 3) == 0) {
                uint64_t values[4];
                if (sscanf(buffer, "cpu %llu %llu %llu %llu", &values[0], &values[1], &values[2], &values[3]) == 4) {
                    entropy.user_time = values[0];
                    //value 1 is nice_time, it looked like a bad source so I ignored it.
                    entropy.system_time = values[2];
                    entropy.idle_time = values[3];
                    break;
                } else {
                    perror("Error parsing CPU stats");
                }
            }
        }
        fclose(fp);
    }
}

// Replacement for get_clock_jitter using CLOCK_MONOTONIC
unsigned long long int get_clock_jitter() {
    if (get_failure() == 0) {
        struct timespec ts;
        if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
            perror("Error getting CLOCK_MONOTONIC time");
            fallback();
            return 0;
        }
        return ((unsigned long long int)ts.tv_sec * 1000000000ULL) + ts.tv_nsec;
    }
    return 0;
}

// Replacement for get_hardware_counter using CLOCK_MONOTONIC_RAW for better accuracy
unsigned long long int get_hardware_counter() {
    if (get_failure() == 0) {
        struct timespec ts;
        if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) != 0) {
            perror("Error getting CLOCK_MONOTONIC_RAW time");
            fallback();
            return 0;
        }
        return ((unsigned long long int)ts.tv_sec * 1000000000ULL) + ts.tv_nsec;
    }
    return 0;
}

// Collects thermal entropy from the temperature sensor
void thermal_entropy(unsigned long long *entropy_value) {
    if (get_failure() == 0) {
        FILE *fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
        if (fp) {
            int temp;
            if (fscanf(fp, "%d", &temp) == 1) {
                *entropy_value ^= (unsigned long long)temp;
            } else {
                perror("Error reading temperature");
            }
            fclose(fp);
        } else {
            perror("Error opening temperature sensor");
            fallback();
        }
    }
}

// Collects entropy from system load averages
void system_load_entropy(unsigned long long *entropy_value) {
    if (get_failure() == 0) {
        double loadavg[3];
        FILE *fp = fopen("/proc/loadavg", "r");
        if (fp) {
            if (fscanf(fp, "%lf %lf %lf", &loadavg[0], &loadavg[1], &loadavg[2]) == 3) {
                const unsigned long LOAD_AVG_MULTIPLIER = 10000;
                *entropy_value ^= (unsigned long)(loadavg[0] * LOAD_AVG_MULTIPLIER); // simple transformation
                *entropy_value ^= (unsigned long)(loadavg[1] * LOAD_AVG_MULTIPLIER);
                *entropy_value ^= (unsigned long)(loadavg[2] * LOAD_AVG_MULTIPLIER);
            } else {
                perror("Error reading load averages");
            }
            fclose(fp);
        } else {
            perror("Error opening /proc/loadavg");
            fallback();
        }
    }
}

// Collects entropy by combining all sources
unsigned long long int collect_entropy() {
    unsigned long long int entropy_value = 0;

    for (int i = 0; i < 1000; i++) {
        if (get_failure() != 0) break; // Stop if any failure occurs

        entropy_value ^= get_clock_jitter();
        entropy_value ^= get_hardware_counter();

        cpu_noise();
        if (get_failure() == 0) { // Only use entropy if cpu_noise succeeded
            entropy_value ^= entropy.user_time;
            entropy_value ^= entropy.system_time;
            entropy_value ^= entropy.idle_time;
        }

        thermal_entropy(&entropy_value);
        system_load_entropy(&entropy_value);
    }

    return entropy_value;
}

void seed_rng(unsigned char *seed, size_t seed_size) {
    unsigned long long int entropy = collect_entropy();
    SHA256((unsigned char*)&entropy, sizeof(entropy), seed); // Use SHA-256 to create a seed
}
