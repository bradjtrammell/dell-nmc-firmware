#include "ou_shut_request.h"

int ou_shut_build_get_report_sized(uint8_t report_id, uint16_t report_bytes,
                                   uint8_t *frame, size_t capacity,
                                   size_t *written) {
    uint8_t checksum=0U;
    unsigned i;
    if (!written) return -1;
    *written=0U;
    if (!frame || capacity<OU_SHUT_GET_REPORT_FRAME_SIZE ||
        report_bytes>OU_SHUT_GET_REPORT_MAX_LENGTH) return -1;
    /* Last REQUEST: type 0x81, size nibble 0x88, HID feature GET REPORT.
     * Interface 0; requested length in little-endian bytes 6 and 7. */
    frame[0]=0x81U;
    frame[1]=0x88U;
    frame[2]=0xa1U;
    frame[3]=0x01U;
    frame[4]=report_id;
    frame[5]=0x03U;
    frame[6]=0U;
    frame[7]=0U;
    frame[8]=(uint8_t)(report_bytes & UINT16_C(0xff));
    frame[9]=(uint8_t)(report_bytes >> 8U);
    for (i=2U;i<10U;++i) checksum^=frame[i];
    frame[10]=checksum;
    *written=OU_SHUT_GET_REPORT_FRAME_SIZE;
    return 0;
}

int ou_shut_build_get_report(uint8_t report_id, uint8_t *frame,
                             size_t capacity, size_t *written) {
    return ou_shut_build_get_report_sized(report_id, 0U, frame, capacity,
                                          written);
}
