/***********************************************************************************************************************
 *
 *            File: comm.c
 *      Created on: Mar 10, 2026
 *          Author: Tomasz Ziajko
 *
 *      All rights reserved
 *
***********************************************************************************************************************/

/**************************************           INCLUDE FILES              ******************************************/
#include "comm.h"
#include "stm32h5xx_hal.h"
#include "string.h"

/**************************************           DEFINES                    ******************************************/
#define COMM_SOF                0xAAU

#define MSG_FIREPLACE_ENABLE    0x01U
#define MSG_FIREPLACE_DISABLE   0x02U
#define MSG_VENTILATION_ENABLE  0x03U
#define MSG_VENTILATION_DISABLE 0x04U
#define MSG_TIME_REQUEST        0x05U

#define MSG_ACK                 0x81U
#define MSG_NACK                0x82U
#define MSG_TIME_RESPONSE       0x83U

/**************************************           DATA TYPES                 ******************************************/

/**************************************           CONSTANTS                  ******************************************/

/**************************************           LOCAL VARIABLES            ******************************************/

/**************************************      LOCAL FUNCTION DECLARATIONS     ******************************************/
static uint16_t crc16_modbus(const uint8_t *data, uint8_t len);
static void     build_frame(comm_data_t *this, uint8_t msg_id, const uint8_t *payload, uint8_t len);
static void     queue_ack(comm_data_t *this, uint8_t echoed_msg_id);
static void     queue_nack(comm_data_t *this, uint8_t echoed_msg_id);
static void     queue_time_response(comm_data_t *this);
static void     process_frame(comm_data_t *this);

/**************************************      GLOBAL FUNCTION DEFINITIONS     ******************************************/
void comm_init(comm_data_t *this,
               int (*uart_transmit)(uint8_t *data, uint16_t size),
               int (*uart_receive)(uint8_t *data, uint16_t size),
               time_data_t *(*get_time)(void),
               void (*on_fireplace_enable)(void),
               void (*on_fireplace_disable)(void),
               void (*on_ventilation_enable)(void),
               void (*on_ventilation_disable)(void))
{
    this->uart_transmit          = uart_transmit;
    this->uart_receive           = uart_receive;
    this->get_time               = get_time;
    this->on_fireplace_enable    = on_fireplace_enable;
    this->on_fireplace_disable   = on_fireplace_disable;
    this->on_ventilation_enable  = on_ventilation_enable;
    this->on_ventilation_disable = on_ventilation_disable;

    this->rx_state       = COMM_RX_SOF;
    this->rx_payload_idx = 0;
    this->frame_ready    = false;
    this->tx_in_progress = false;
    this->tx_pending     = false;

    this->uart_receive(&this->rx_byte, 1);
}

void comm_main(comm_data_t *this)
{
    if (this->frame_ready)
    {
        this->frame_ready = false;
        process_frame(this);
    }

    if (this->tx_pending && !this->tx_in_progress)
    {
        this->tx_pending     = false;
        this->tx_in_progress = true;
        this->uart_transmit(this->tx_buffer, this->tx_size);
    }
}

void comm_rx_interrupt(comm_data_t *this)
{
    uint8_t byte = this->rx_byte;

    switch (this->rx_state)
    {
        case COMM_RX_SOF:
            if (byte == COMM_SOF)
            {
                this->rx_state = COMM_RX_MSG_ID;
            }
            break;

        case COMM_RX_MSG_ID:
            this->rx_msg_id = byte;
            this->rx_state  = COMM_RX_LEN;
            break;

        case COMM_RX_LEN:
            this->rx_len         = byte;
            this->rx_payload_idx = 0;
            if (this->rx_len > sizeof(this->rx_payload))
            {
                // Oversized payload — discard and re-sync
                this->rx_state = COMM_RX_SOF;
            }
            else if (this->rx_len > 0)
            {
                this->rx_state = COMM_RX_PAYLOAD;
            }
            else
            {
                this->rx_state = COMM_RX_CRC_HIGH;
            }
            break;

        case COMM_RX_PAYLOAD:
            this->rx_payload[this->rx_payload_idx++] = byte;
            if (this->rx_payload_idx >= this->rx_len)
            {
                this->rx_state = COMM_RX_CRC_HIGH;
            }
            break;

        case COMM_RX_CRC_HIGH:
            this->rx_crc_bytes[0] = byte;
            this->rx_state        = COMM_RX_CRC_LOW;
            break;

        case COMM_RX_CRC_LOW:
        {
            this->rx_crc_bytes[1] = byte;

            // Validate CRC (covers MSG_ID + LEN + PAYLOAD)
            uint8_t crc_buf[2 + 8];
            crc_buf[0] = this->rx_msg_id;
            crc_buf[1] = this->rx_len;
            memcpy(&crc_buf[2], this->rx_payload, this->rx_len);

            uint16_t computed = crc16_modbus(crc_buf, 2U + this->rx_len);
            uint16_t received = ((uint16_t)this->rx_crc_bytes[0] << 8) | this->rx_crc_bytes[1];

            if (computed == received)
            {
                // Latch into stable frame buffer for comm_main()
                this->frame_msg_id = this->rx_msg_id;
                this->frame_len    = this->rx_len;
                memcpy(this->frame_payload, this->rx_payload, this->rx_len);
                this->frame_ready  = true;
            }

            this->rx_state = COMM_RX_SOF;
            break;
        }
    }

    // Re-arm single-byte receive for the next byte
    this->uart_receive(&this->rx_byte, 1);
}

void comm_tx_interrupt(comm_data_t *this)
{
    this->tx_in_progress = false;
}

/**************************************      LOCAL FUNCTION DEFINITIONS      ******************************************/
static uint16_t crc16_modbus(const uint8_t *data, uint8_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint8_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
        {
            crc = (crc & 0x0001U) ? (crc >> 1) ^ 0xA001U : (crc >> 1);
        }
    }
    return crc;
}

static void build_frame(comm_data_t *this, uint8_t msg_id, const uint8_t *payload, uint8_t len)
{
    this->tx_buffer[0] = COMM_SOF;
    this->tx_buffer[1] = msg_id;
    this->tx_buffer[2] = len;
    if (len > 0 && payload != NULL)
    {
        memcpy(&this->tx_buffer[3], payload, len);
    }

    uint8_t crc_buf[2 + 8];
    crc_buf[0] = msg_id;
    crc_buf[1] = len;
    if (len > 0 && payload != NULL)
    {
        memcpy(&crc_buf[2], payload, len);
    }
    uint16_t crc = crc16_modbus(crc_buf, 2U + len);

    this->tx_buffer[3 + len] = (crc >> 8) & 0xFF;
    this->tx_buffer[4 + len] =  crc        & 0xFF;
    this->tx_size = 5U + len;
}

static void queue_ack(comm_data_t *this, uint8_t echoed_msg_id)
{
    uint8_t payload[1] = { echoed_msg_id };
    build_frame(this, MSG_ACK, payload, 1);
    this->tx_pending = true;
}

static void queue_nack(comm_data_t *this, uint8_t echoed_msg_id)
{
    uint8_t payload[1] = { echoed_msg_id };
    build_frame(this, MSG_NACK, payload, 1);
    this->tx_pending = true;
}

static void queue_time_response(comm_data_t *this)
{
    time_data_t *t = this->get_time();
    uint16_t full_year = 2000U + t->year;

    uint8_t payload[7];
    payload[0] = (full_year >> 8) & 0xFF;
    payload[1] =  full_year       & 0xFF;
    payload[2] = t->month;
    payload[3] = t->day_of_month;
    payload[4] = t->hour;
    payload[5] = t->minute;
    payload[6] = t->second;

    build_frame(this, MSG_TIME_RESPONSE, payload, 7);
    this->tx_pending = true;
}

static void process_frame(comm_data_t *this)
{
    switch (this->frame_msg_id)
    {
        case MSG_FIREPLACE_ENABLE:
            if (this->on_fireplace_enable != NULL) { this->on_fireplace_enable(); }
            queue_ack(this, MSG_FIREPLACE_ENABLE);
            break;

        case MSG_FIREPLACE_DISABLE:
            if (this->on_fireplace_disable != NULL) { this->on_fireplace_disable(); }
            queue_ack(this, MSG_FIREPLACE_DISABLE);
            break;

        case MSG_VENTILATION_ENABLE:
            if (this->on_ventilation_enable != NULL) { this->on_ventilation_enable(); }
            queue_ack(this, MSG_VENTILATION_ENABLE);
            break;

        case MSG_VENTILATION_DISABLE:
            if (this->on_ventilation_disable != NULL) { this->on_ventilation_disable(); }
            queue_ack(this, MSG_VENTILATION_DISABLE);
            break;

        case MSG_TIME_REQUEST:
            queue_time_response(this);
            break;

        default:
            queue_nack(this, this->frame_msg_id);
            break;
    }
}
