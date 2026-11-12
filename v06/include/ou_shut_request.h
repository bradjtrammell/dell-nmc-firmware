#ifndef OU_SHUT_REQUEST_H
#define OU_SHUT_REQUEST_H
#include <stddef.h>
#include <stdint.h>

/* Published SHUT GET REPORT operation A1 01 with HID report ID and feature
 * type 03. Pure builder ONLY: no hardware writes, no UART, no power commands.
 * The Dell H910P internal UPS link has not been electrically qualified. */
#define OU_SHUT_GET_REPORT_FRAME_SIZE 11U
int ou_shut_build_get_report(uint8_t report_id, uint8_t *frame,
                             size_t capacity, size_t *written);
#endif
