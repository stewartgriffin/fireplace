/***********************************************************************************************************************
 *
 *            File: comm.h
 *      Created on: Mar 10, 2026
 *          Author: Tomasz Ziajko
 *
 *      All right reserved.
 *
***********************************************************************************************************************/

#ifndef COMPONENTS_COMM_COMM_H_
#define COMPONENTS_COMM_COMM_H_

/**************************************           INCLUDE FILES              ******************************************/
#include "stdint.h"
#include "stdbool.h"
#include "ds3231.h"

/* CPP GUARD BEGIN */
#ifdef __cplusplus
extern "C" {
#endif

/**************************************           DATA TYPES                 ******************************************/

/**
 * @brief RX frame parser state machine states
 */
typedef enum
{
    COMM_RX_SOF,       ///< Waiting for start-of-frame byte (0xAA)
    COMM_RX_MSG_ID,    ///< Waiting for message ID byte
    COMM_RX_LEN,       ///< Waiting for payload length byte
    COMM_RX_PAYLOAD,   ///< Accumulating payload bytes
    COMM_RX_CRC_HIGH,  ///< Waiting for CRC high byte
    COMM_RX_CRC_LOW,   ///< Waiting for CRC low byte; validates and latches frame
} comm_rx_state_t;

/**
 * @brief COMM RS485 driver data structure
 *
 * Implements a request/response protocol over UART RS485 half-duplex.
 * Receives commands from the display (fireplace/ventilation enable/disable,
 * time request) and responds with ACK, NACK, or a time response frame.
 *
 * Frame format: [SOF=0xAA][MSG_ID][LEN][PAYLOAD...][CRC_HIGH][CRC_LOW]
 * CRC16-Modbus covers: [MSG_ID][LEN][PAYLOAD...]
 */
typedef struct
{
    /**
     * @brief Start interrupt-driven UART transmit
     * @param data Pointer to data buffer
     * @param size Number of bytes to send
     * @return 0 on success, error code otherwise
     */
    int (*uart_transmit)(uint8_t *data, uint16_t size);

    /**
     * @brief Start interrupt-driven UART receive (typically 1 byte)
     * @param data Pointer to receive buffer
     * @param size Number of bytes to receive
     * @return 0 on success, error code otherwise
     */
    int (*uart_receive)(uint8_t *data, uint16_t size);

    /**
     * @brief Get current RTC time
     * @return Pointer to current time_data_t
     */
    time_data_t *(*get_time)(void);

    /** @brief Called when a valid fireplace enable command is received */
    void (*on_fireplace_enable)(void);

    /** @brief Called when a valid fireplace disable command is received */
    void (*on_fireplace_disable)(void);

    /** @brief Called when a valid ventilation enable command is received */
    void (*on_ventilation_enable)(void);

    /** @brief Called when a valid ventilation disable command is received */
    void (*on_ventilation_disable)(void);

    /* Internal state — do not modify directly */
    comm_rx_state_t rx_state;        ///< RX frame parser state
    uint8_t rx_byte;                 ///< Single-byte buffer used with uart_receive
    uint8_t rx_msg_id;               ///< MSG_ID of the frame being received
    uint8_t rx_len;                  ///< Payload length of the frame being received
    uint8_t rx_payload[8];           ///< Payload byte accumulator
    uint8_t rx_payload_idx;          ///< Number of payload bytes received so far
    uint8_t rx_crc_bytes[2];         ///< CRC byte accumulator

    bool    frame_ready;             ///< Set in ISR when a CRC-validated frame is available
    uint8_t frame_msg_id;            ///< MSG_ID of the latched frame
    uint8_t frame_len;               ///< Payload length of the latched frame
    uint8_t frame_payload[8];        ///< Payload of the latched frame

    bool    tx_in_progress;          ///< UART TX transfer is currently active
    bool    tx_pending;              ///< A frame is queued and waiting to be sent
    uint8_t tx_buffer[13];           ///< TX frame buffer (5 overhead + 8 max payload)
    uint8_t tx_size;                 ///< Number of bytes in tx_buffer to transmit
} comm_data_t;

/**************************************           DEFINES                    ******************************************/

/**************************************    GLOBAL FUNCTION DECLARATIONS      ******************************************/

/**
 * @brief Initialise the COMM RS485 driver and arm the first UART receive
 * @param this                  Pointer to comm data structure
 * @param uart_transmit         Function pointer for interrupt-driven UART TX
 * @param uart_receive          Function pointer for interrupt-driven UART RX
 * @param get_time              Function pointer returning current RTC time
 * @param on_fireplace_enable   Callback invoked on fireplace enable command
 * @param on_fireplace_disable  Callback invoked on fireplace disable command
 * @param on_ventilation_enable  Callback invoked on ventilation enable command
 * @param on_ventilation_disable Callback invoked on ventilation disable command
 */
void comm_init(comm_data_t *this,
               int (*uart_transmit)(uint8_t *data, uint16_t size),
               int (*uart_receive)(uint8_t *data, uint16_t size),
               time_data_t *(*get_time)(void),
               void (*on_fireplace_enable)(void),
               void (*on_fireplace_disable)(void),
               void (*on_ventilation_enable)(void),
               void (*on_ventilation_disable)(void));

/**
 * @brief Main function — call from the main loop to process received frames
 * and transmit queued responses
 * @param this Pointer to comm data structure
 */
void comm_main(comm_data_t *this);

/**
 * @brief UART RX complete interrupt callback
 * Call from HAL_UART_RxCpltCallback when the UART used for comm completes a receive
 * @param this Pointer to comm data structure
 */
void comm_rx_interrupt(comm_data_t *this);

/**
 * @brief UART TX complete interrupt callback
 * Call from HAL_UART_TxCpltCallback when the UART used for comm completes a transmit
 * @param this Pointer to comm data structure
 */
void comm_tx_interrupt(comm_data_t *this);

/* CPP GUARD END */
#ifdef __cplusplus
}
#endif

#endif /* COMPONENTS_COMM_COMM_H_ */
