#ifndef ASSET_SOURCE_H
#define ASSET_SOURCE_H

typedef struct asset_source asset_source_t;

typedef enum asset_source_state {
    ASSET_SOURCE_PENDING,
    ASSET_SOURCE_COMPLETE,
    ASSET_SOURCE_ERROR
} asset_source_state_t;

asset_source_t *asset_source_start(const char *asset_path,
        const char *cache_path);
asset_source_state_t asset_source_get_state(asset_source_t *source);
const uint8_t *asset_source_get_data(asset_source_t *source, size_t *size);
const char *asset_source_get_error(const asset_source_t *source);
char *asset_source_speedinfo(asset_source_t *source, char *buffer, size_t size);
void asset_source_free(asset_source_t *source);

#endif
