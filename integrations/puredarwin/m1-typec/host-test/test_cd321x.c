/* SPDX-License-Identifier: GPL-2.0 */
#include "pd_cd321x.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    uint8_t wire[65], out[66];
    for (unsigned n = 1; n <= 64; ++n) {
        for (unsigned i = 1; i < sizeof(wire); ++i) wire[i] = (uint8_t)(i * 17);
        for (unsigned advertised = 0; advertised <= 255; ++advertised) {
            wire[0] = (uint8_t)advertised;
            memset(out, 0xa5, sizeof(out));
            int ret = pd_cd321x_unpack(wire, n + 1, out + 1, n);
            assert(ret == (advertised < n ? EIO : 0));
            assert(out[0] == 0xa5 && out[n + 1] == 0xa5);
            if (!ret) assert(memcmp(out + 1, wire + 1, n) == 0);
            else for (unsigned i = 0; i < sizeof(out); ++i) assert(out[i] == 0xa5);
        }
        wire[0] = (uint8_t)n;
        for (unsigned size = 0; size <= n; ++size) {
            memset(out, 0xa5, sizeof(out));
            assert(pd_cd321x_unpack(wire, size, out, n) == EIO);
            for (unsigned i = 0; i < sizeof(out); ++i) assert(out[i] == 0xa5);
        }
    }
    assert(pd_cd321x_unpack(NULL, 65, out, 4) == EINVAL);
    assert(pd_cd321x_unpack(wire, 65, NULL, 4) == EINVAL);
    assert(pd_cd321x_unpack(wire, 65, out, 0) == EINVAL);
    assert(pd_cd321x_unpack(wire, 65, out, 65) == EINVAL);
    const uint8_t word[] = {0xef, 0xcd, 0xab, 0x89};
    assert(pd_cd321x_le32(word) == 0x89abcdefu);
    puts("CD321x register framing: 16384 lengths + 2080 truncations passed (host only)");
    return 0;
}
