/**
 * @file
 * Aggregated operational metrics for direct networking and the game loop.
 */

#include <global.h>
#include <network_metrics.h>
#include <toolkit/string.h>

#define METRICS_SAMPLES 2048

static pthread_mutex_t metrics_lock = PTHREAD_MUTEX_INITIALIZER;
static struct {
    uint64_t accepted;
    uint64_t rejected;
    size_t pending;
    size_t pending_peak;
    uint64_t quic_service_calls;
    uint64_t quic_network_ready_calls;
    size_t queue_bytes;
    size_t queue_peak_bytes;
    size_t connection_queue_peak_bytes;
    uint64_t queue_rejected;
    size_t asset_cache_bytes;
    uint64_t asset_responses;
    uint64_t asset_paced;
    size_t asset_streams_active;
    size_t asset_streams_peak;
    uint64_t asset_stream_rejected;
    uint64_t asset_stream_bytes;
    uint64_t asset_latency_us[METRICS_SAMPLES];
    size_t asset_latency_count;
    size_t asset_latency_next;
    char mapping_method[32];
    uint64_t mapping_open_failures;
    uint64_t mapping_renewal_failures;
    uint64_t game_loop_us[METRICS_SAMPLES];
    size_t game_loop_count;
    size_t game_loop_next;
} metrics;

static int metrics_compare_u64(const void *left, const void *right) {
    uint64_t a = *(const uint64_t *)left;
    uint64_t b = *(const uint64_t *)right;
    return a > b ? 1 : a < b ? -1 : 0;
}

static uint64_t metrics_percentile(uint64_t *samples, size_t count, unsigned int percentile) {
    if (count == 0) {
        return 0;
    }
    qsort(samples, count, sizeof(*samples), metrics_compare_u64);
    size_t index = (count - 1) * percentile / 100;
    return samples[index];
}

void server_metrics_pending_changed(size_t pending) {
    pthread_mutex_lock(&metrics_lock);
    metrics.pending = pending;
    metrics.pending_peak = MAX(metrics.pending_peak, pending);
    pthread_mutex_unlock(&metrics_lock);
}

void server_metrics_connection_accepted(size_t pending) {
    pthread_mutex_lock(&metrics_lock);
    metrics.accepted++;
    metrics.pending = pending;
    metrics.pending_peak = MAX(metrics.pending_peak, pending);
    pthread_mutex_unlock(&metrics_lock);
}

void server_metrics_connection_rejected(size_t pending) {
    pthread_mutex_lock(&metrics_lock);
    metrics.rejected++;
    metrics.pending = pending;
    pthread_mutex_unlock(&metrics_lock);
}

void server_metrics_quic_service(bool network_ready) {
    pthread_mutex_lock(&metrics_lock);
    metrics.quic_service_calls++;
    if (network_ready) {
        metrics.quic_network_ready_calls++;
    }
    pthread_mutex_unlock(&metrics_lock);
}

void server_metrics_queue_changed(int64_t delta, size_t connection_bytes, bool rejected) {
    pthread_mutex_lock(&metrics_lock);
    if (delta < 0) {
        size_t removed = (size_t)-delta;
        metrics.queue_bytes = removed <= metrics.queue_bytes ? metrics.queue_bytes - removed : 0;
    } else {
        metrics.queue_bytes += (size_t)delta;
    }
    metrics.queue_peak_bytes = MAX(metrics.queue_peak_bytes, metrics.queue_bytes);
    metrics.connection_queue_peak_bytes =
        MAX(metrics.connection_queue_peak_bytes, connection_bytes);
    if (rejected) {
        metrics.queue_rejected++;
    }
    pthread_mutex_unlock(&metrics_lock);
}

void server_metrics_asset_cache(size_t bytes) {
    pthread_mutex_lock(&metrics_lock);
    metrics.asset_cache_bytes = bytes;
    pthread_mutex_unlock(&metrics_lock);
}

void server_metrics_asset_response(uint64_t latency_us) {
    pthread_mutex_lock(&metrics_lock);
    metrics.asset_responses++;
    metrics.asset_latency_us[metrics.asset_latency_next++] = latency_us;
    metrics.asset_latency_next %= METRICS_SAMPLES;
    metrics.asset_latency_count = MIN(metrics.asset_latency_count + 1, (size_t)METRICS_SAMPLES);
    pthread_mutex_unlock(&metrics_lock);
}

void server_metrics_asset_paced(void) {
    pthread_mutex_lock(&metrics_lock);
    metrics.asset_paced++;
    pthread_mutex_unlock(&metrics_lock);
}

void server_metrics_asset_stream(int active_delta, size_t bytes, bool rejected) {
    pthread_mutex_lock(&metrics_lock);
    if (active_delta < 0) {
        size_t removed = (size_t)-active_delta;
        metrics.asset_streams_active =
            removed <= metrics.asset_streams_active ? metrics.asset_streams_active - removed : 0;
    } else {
        metrics.asset_streams_active += (size_t)active_delta;
    }
    metrics.asset_streams_peak = MAX(metrics.asset_streams_peak, metrics.asset_streams_active);
    metrics.asset_stream_bytes += bytes;
    if (rejected) {
        metrics.asset_stream_rejected++;
    }
    pthread_mutex_unlock(&metrics_lock);
}

void server_metrics_mapping(const char *method, bool open_failed, bool renewal_failed) {
    pthread_mutex_lock(&metrics_lock);
    if (method != NULL) {
        snprintf(VS(metrics.mapping_method), "%s", method);
    }
    if (open_failed) {
        metrics.mapping_open_failures++;
    }
    if (renewal_failed) {
        metrics.mapping_renewal_failures++;
    }
    pthread_mutex_unlock(&metrics_lock);
}

void server_metrics_game_loop(uint64_t duration_us) {
    pthread_mutex_lock(&metrics_lock);
    metrics.game_loop_us[metrics.game_loop_next++] = duration_us;
    metrics.game_loop_next %= METRICS_SAMPLES;
    metrics.game_loop_count = MIN(metrics.game_loop_count + 1, (size_t)METRICS_SAMPLES);
    pthread_mutex_unlock(&metrics_lock);
}

void server_metrics_stats(char *buffer, size_t size) {
    uint64_t game[METRICS_SAMPLES];
    uint64_t asset[METRICS_SAMPLES];

    pthread_mutex_lock(&metrics_lock);
    size_t game_count = metrics.game_loop_count;
    size_t asset_count = metrics.asset_latency_count;
    memcpy(game, metrics.game_loop_us, game_count * sizeof(*game));
    memcpy(asset, metrics.asset_latency_us, asset_count * sizeof(*asset));

    snprintfcat(buffer, size, "\n=== NETWORK ===\n");
    snprintfcat(buffer,
                size,
                "\nConnections: accepted=%" PRIu64 " rejected=%" PRIu64 " pending=%" PRIu64
                " pending_peak=%" PRIu64,
                metrics.accepted,
                metrics.rejected,
                (uint64_t)metrics.pending,
                (uint64_t)metrics.pending_peak);
    snprintfcat(buffer,
                size,
                "\nQUIC service: calls=%" PRIu64 " network-ready=%" PRIu64 " calls-per-ready=%.2f",
                metrics.quic_service_calls,
                metrics.quic_network_ready_calls,
                metrics.quic_network_ready_calls != 0
                    ? (double)metrics.quic_service_calls / (double)metrics.quic_network_ready_calls
                    : 0.0);
    snprintfcat(buffer,
                size,
                "\nOutbound queues: bytes=%" PRIu64 " aggregate_peak=%" PRIu64
                " connection_peak=%" PRIu64 " rejected=%" PRIu64,
                (uint64_t)metrics.queue_bytes,
                (uint64_t)metrics.queue_peak_bytes,
                (uint64_t)metrics.connection_queue_peak_bytes,
                metrics.queue_rejected);
    snprintfcat(buffer,
                size,
                "\nAssets: cached_rss=%" PRIu64 " responses=%" PRIu64 " paced=%" PRIu64
                " streams=%" PRIu64 " stream_peak=%" PRIu64 " stream_rejected=%" PRIu64
                " body_bytes=%" PRIu64,
                (uint64_t)metrics.asset_cache_bytes,
                metrics.asset_responses,
                metrics.asset_paced,
                (uint64_t)metrics.asset_streams_active,
                (uint64_t)metrics.asset_streams_peak,
                metrics.asset_stream_rejected,
                metrics.asset_stream_bytes);
    snprintfcat(buffer,
                size,
                "\nMapping: method=%s open_failures=%" PRIu64 " renewal_failures=%" PRIu64,
                *metrics.mapping_method != '\0' ? metrics.mapping_method : "none",
                metrics.mapping_open_failures,
                metrics.mapping_renewal_failures);
    pthread_mutex_unlock(&metrics_lock);

    snprintfcat(buffer,
                size,
                "\nAsset latency us: p50=%" PRIu64 " p95=%" PRIu64 " p99=%" PRIu64,
                metrics_percentile(asset, asset_count, 50),
                metrics_percentile(asset, asset_count, 95),
                metrics_percentile(asset, asset_count, 99));
    snprintfcat(buffer,
                size,
                "\nGame loop us: p50=%" PRIu64 " p95=%" PRIu64 " p99=%" PRIu64 "\n",
                metrics_percentile(game, game_count, 50),
                metrics_percentile(game, game_count, 95),
                metrics_percentile(game, game_count, 99));
}
