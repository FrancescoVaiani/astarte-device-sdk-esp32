/*
 * (C) Copyright 2019-2021, Ispirata Srl, info@ispirata.com.
 *
 * SPDX-License-Identifier: LGPL-2.1+ OR Apache-2.0
 */

#include "uuid.h"

#include <esp_log.h>
#include <esp_system.h>

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

#include <mbedtls/md.h>

#define UUID_LEN 16
#define UUID_STR_LEN 36

#define TAG "uuid"

// Helper struct to make it easier to access UUID fields
struct uuid
{
    uint32_t time_low;
    uint16_t time_mid;
    uint16_t time_hi_and_version;
    uint8_t clock_seq_hi_res;
    uint8_t clock_seq_low;
    uint8_t node[6];
};

static void uuid_from_struct(const struct uuid *in, uuid_t out)
{
    uint32_t tmp32;
    uint16_t tmp16;
    uint8_t *out_p = out;

    tmp32 = htonl(in->time_low);
    memcpy(out_p, &tmp32, sizeof(uint32_t));

    tmp16 = htons(in->time_mid);
    memcpy(out_p + 4, &tmp16, sizeof(uint16_t));

    tmp16 = htons(in->time_hi_and_version);
    memcpy(out_p + 6, &tmp16, sizeof(uint16_t));

    out_p[8] = in->clock_seq_hi_res;
    out_p[9] = in->clock_seq_low;

    memcpy(out_p + 10, in->node, 6);
}

static void uuid_to_struct(const uuid_t in, struct uuid *out)
{
    uint32_t tmp32;
    uint16_t tmp16;
    const uint8_t *in_p = in;

    memcpy(&tmp32, in_p, sizeof(uint32_t));
    out->time_low = ntohl(tmp32);

    memcpy(&tmp16, in_p + 4, sizeof(uint16_t));
    out->time_mid = ntohs(tmp16);

    memcpy(&tmp16, in_p + 6, sizeof(uint16_t));
    out->time_hi_and_version = ntohs(tmp16);

    out->clock_seq_hi_res = in_p[8];
    out->clock_seq_low = in_p[9];

    memcpy(out->node, in_p + 10, 6);
}

void uuid_generate_v5(const uuid_t ns, const void *data, size_t length, uuid_t out)
{
    uint8_t sha_result[32];

    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA1;

    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
    mbedtls_md_starts(&ctx);
    mbedtls_md_update(&ctx, ns, UUID_LEN);
    mbedtls_md_update(&ctx, data, length);
    mbedtls_md_finish(&ctx, sha_result);
    mbedtls_md_free(&ctx);

    struct uuid sha_uuid_struct;
    // This will use the first 16 bytes of the SHA
    uuid_to_struct(sha_result, &sha_uuid_struct);

    int version = 5;
    sha_uuid_struct.time_hi_and_version &= 0x0FFF;
    sha_uuid_struct.time_hi_and_version |= (version << 12);
    sha_uuid_struct.clock_seq_hi_res &= 0x3F;
    sha_uuid_struct.clock_seq_hi_res |= 0x80;

    uuid_from_struct(&sha_uuid_struct, out);
}

void uuid_to_string(const uuid_t uuid, char *out)
{
    struct uuid uuid_struct;

    uuid_to_struct(uuid, &uuid_struct);
    sprintf(out, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x", uuid_struct.time_low,
        uuid_struct.time_mid, uuid_struct.time_hi_and_version, uuid_struct.clock_seq_hi_res,
        uuid_struct.clock_seq_low, uuid_struct.node[0], uuid_struct.node[1], uuid_struct.node[2],
        uuid_struct.node[3], uuid_struct.node[4], uuid_struct.node[5]);
}

int uuid_from_string(const char *in, uuid_t uuid)
{
    // Length check
    if (strlen(in) != UUID_STR_LEN) {
        return -1;
    }

    // Sanity check
    for (int i = 0; i < 36; i++) {
        char c = in[i];
        // Check that hyphens are in the right place
        if ((i == 8) || (i == 13) || (i == 18) || (i == 23)) {
            if (c != '-') {
                ESP_LOGW(TAG, "Found invalid character %c in hyphen position %d", c, i);
                return -1;
            } else {
                continue;
            }
        }

        // Check that everything else is an hexadecimal digit
        if (!((c >= '0') && (c <= '9')) && !((c >= 'a') && (c <= 'f'))
            && !((c >= 'A') && (c <= 'F'))) {
            ESP_LOGW(TAG, "Found invalid character %c in position %d", c, i);
            return -1;
        }
    }

    char tmp[3];
    struct uuid uuid_struct;

    uuid_struct.time_low = strtoul(in, NULL, 16);
    uuid_struct.time_mid = strtoul(in + 9, NULL, 16);
    uuid_struct.time_hi_and_version = strtoul(in + 14, NULL, 16);

    tmp[0] = in[19];
    tmp[1] = in[20];
    uuid_struct.clock_seq_hi_res = strtoul(tmp, NULL, 16);

    tmp[0] = in[21];
    tmp[1] = in[22];
    uuid_struct.clock_seq_low = strtoul(tmp, NULL, 16);

    for (int i = 0; i < 6; i++) {
        tmp[0] = in[24 + i * 2];
        tmp[1] = in[24 + i * 2 + 1];
        uuid_struct.node[i] = strtoul(tmp, NULL, 16);
    }

    uuid_from_struct(&uuid_struct, uuid);
    return 0;
}

void uuid_generate_v4(uuid_t out)
{
    uint8_t random_result[16];
    esp_fill_random(random_result, 16);

    struct uuid random_uuid_struct;
    uuid_to_struct(random_result, &random_uuid_struct);

    int version = 4;
    random_uuid_struct.time_hi_and_version &= 0x0FFF;
    random_uuid_struct.time_hi_and_version |= (version << 12);

    uuid_from_struct(&random_uuid_struct, out);
}
