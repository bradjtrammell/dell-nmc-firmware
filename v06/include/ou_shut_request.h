#ifndef OU_SHUT_REQUEST_H
#define OU_SHUT_REQUEST_H
#include <stddef.h>
#include <stdint.h>

/* Published SHUT HID Feature GET REPORT: A1 01, report ID, type 03,
 * interface 0, requested report length (LSB first).
 * Pure wire builder: no UART writes or physical UPS operation.
 * An H910P electrical path has NOT been independently verified. */
#define OU_SHUT_GET_REPORT_FRAME_SIZE 11U
#define OU_SHUT_GET_REPORT_MAX_LENGTH 256U

/* Compatibility form: length=0 per MGE 51029473ZA AC, section 2.5. */
int ou_shut_build_get_report(uint8_t report_id, uint8_t *frame,
                             size_t capacity, size_t *written);

/* Length=0 requests the device's default length. Some published example
 * devices explicitly pass 8; never invent length from an unknown device. */
int ou_shut_build_get_report_sized(uint8_t report_id, uint16_t report_bytes,
                                   uint8_t *frame, size_t capacity,
                                   size_t *written);
#endif
