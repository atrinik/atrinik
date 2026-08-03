/**
 * @file
 * Low-overhead server network and game-loop observability.
 */

#ifndef NETWORK_METRICS_H
#define NETWORK_METRICS_H

#include <toolkit/toolkit.h>

void server_metrics_connection_accepted(size_t pending);
void server_metrics_connection_rejected(size_t pending);
void server_metrics_pending_changed(size_t pending);
void server_metrics_quic_service(bool network_ready);
void server_metrics_queue_changed(int64_t delta,
                                  size_t  connection_bytes,
                                  bool    rejected);
void server_metrics_asset_cache(size_t bytes);
void server_metrics_asset_response(uint64_t latency_us, bool throttled);
void server_metrics_mapping(const char *method,
                            bool        open_failed,
                            bool        renewal_failed);
void server_metrics_game_loop(uint64_t duration_us);
void server_metrics_stats(char *buffer, size_t size);

#endif
