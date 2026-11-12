#include "ou_shut_request.h"

int ou_shut_build_get_report(uint8_t report_id, uint8_t *frame,
                             size_t capacity, size_t *written) {
    uint8_t checksum=0U;
    unsigned i;
    if (!written) return -1;
    *written=0U;
    if (!frame || capacity<OU_SHUT_GET_REPORT_FRAME_SIZE) return -1;
    /* SHUT last REQUEST (0x81), length 0x88 (8 bytes), followed by the
     * HID feature GET REPORT command, then XOR of the data bytes only. */
    frame[0]=0x81U;
    frame[1]=0x88U;
    frame[2]=0xa1U;
    frame[3]=0x01U;
    frame[4]=report_id;
    frame[5]=0x03U;
    frame[6]=0U;
    frame[7]=0U;
    frame[8]=0U;
    frame[9]=0U;
    for (i=2U;i<10U;++i) checksum^=frame[i];
    frame[10]=checksum;
    *written=OU_SHUT_GET_REPORT_FRAME_SIZE;
    return 0;
}
