/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Smart Parking System
  *                   RFID + IR + Ultrasonic + UART
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include "gpio.h"
#include "spi.h"
#include "usart.h"
#include "tim.h"

#include <stdio.h>
#include <string.h>


/* ============================================================
   PIN DEFINITIONS
   ============================================================ */

/* RC522 */

#define RFID_CS_GPIO_Port     GPIOA
#define RFID_CS_Pin           GPIO_PIN_4

#define RFID_RST_GPIO_Port    GPIOB
#define RFID_RST_Pin          GPIO_PIN_0


/* SLOT 1 - IR SENSOR */

#define IR_GPIO_Port          GPIOA
#define IR_Pin                GPIO_PIN_0

#define IR_DETECTED_STATE     GPIO_PIN_RESET


/* SLOT 2 - ULTRASONIC */

#define TRIG_GPIO_Port        GPIOA
#define TRIG_Pin              GPIO_PIN_1

#define ECHO_GPIO_Port        GPIOB
#define ECHO_Pin              GPIO_PIN_1


/* ============================================================
   ULTRASONIC SETTINGS
   ============================================================ */

#define ULTRASONIC_OCCUPIED_CM   13
#define ULTRASONIC_FREE_CM       18


/* ============================================================
   RFID COMMANDS
   ============================================================ */

#define PCD_IDLE              0x00
#define PCD_AUTHENT           0x0E
#define PCD_TRANSCEIVE        0x0C

#define PICC_REQIDL           0x26
#define PICC_ANTICOLL         0x93
#define PICC_HALT             0x50


/* ============================================================
   RC522 REGISTERS
   ============================================================ */

#define CommandReg            0x01
#define CommIEnReg            0x02
#define CommIrqReg            0x04
#define ErrorReg              0x06

#define FIFODataReg           0x09
#define FIFOLevelReg          0x0A

#define ControlReg            0x0C
#define BitFramingReg         0x0D

#define ModeReg               0x11
#define TxControlReg          0x14
#define TxASKReg              0x15

#define TModeReg              0x2A
#define TPrescalerReg         0x2B
#define TReloadRegH           0x2C
#define TReloadRegL           0x2D

#define VersionReg            0x37


/* ============================================================
   RFID STATUS
   ============================================================ */

#define MI_OK                 0
#define MI_NOTAGERR           1
#define MI_ERR                2


/* ============================================================
   RFID UIDs
   ============================================================ */

const uint8_t DK_UID[4] =
{
    0x43,
    0x35,
    0x28,
    0x2A
};

const uint8_t DEVA_UID[4] =
{
    0x03,
    0x56,
    0x71,
    0x11
};


/* ============================================================
   USER STATES
   ============================================================ */

uint8_t DK_inside = 0;
uint8_t DEVA_inside = 0;

uint8_t DK_slot = 0;
uint8_t DEVA_slot = 0;


/* ============================================================
   ULTRASONIC STORED STATE
   ============================================================ */

uint32_t slot2_distance = 999;

uint8_t slot2_occupied = 0;

uint32_t last_ultrasonic_time = 0;


/* ============================================================
   FUNCTION PROTOTYPES
   ============================================================ */

void SystemClock_Config(void);

void UART_Print(const char *msg);
void UART_LogCSV(const char *eventType, const char *user, const char *slot, const char *status);

static void RFID_Select(void);
static void RFID_Deselect(void);

void RFID_WriteRegister(uint8_t reg, uint8_t value);
uint8_t RFID_ReadRegister(uint8_t reg);
void RFID_SetBitMask(uint8_t reg, uint8_t mask);
void RFID_ClearBitMask(uint8_t reg, uint8_t mask);
void RFID_Reset(void);
void RFID_AntennaOn(void);
void RFID_Init(void);
uint8_t RFID_ToCard(uint8_t command, uint8_t *sendData, uint8_t sendLen, uint8_t *backData, uint16_t *backLen);
uint8_t RFID_Request(uint8_t reqMode, uint8_t *tagType);
uint8_t RFID_Anticoll(uint8_t *uid);
void RFID_Halt(void);

void delay_us(uint16_t us);
uint32_t Ultrasonic_GetDistance(void);
void UpdateUltrasonicSlot(void);

uint8_t Slot1_Occupied(void);
uint8_t Slot2_Occupied(void);
uint8_t SlotReserved(uint8_t slot);
uint8_t FindAvailableSlot(void);
void PrintParkingStatus(void);
void ProcessCard(uint8_t *uid);


/* ============================================================
   UART
   ============================================================ */

void UART_Print(const char *msg)
{
    HAL_UART_Transmit(
        &huart1,
        (uint8_t *)msg,
        strlen(msg),
        HAL_MAX_DELAY
    );
}


/* ============================================================
   CSV LOG LINE FOR EXCEL LOGGING
   Format: LOG,<tick_ms>,<event>,<user/uid>,<slot>,<status>
   tick_ms comes from HAL_GetTick() -> ms since boot,
   millisecond-accurate relative timing straight from the MCU.
   ============================================================ */

void UART_LogCSV(const char *eventType,
                 const char *user,
                 const char *slot,
                 const char *status)
{
    char line[100];

    uint32_t tick =
        HAL_GetTick();

    sprintf(
        line,
        "LOG,%lu,%s,%s,%s,%s\r\n",
        (unsigned long)tick,
        eventType,
        user,
        slot,
        status
    );

    UART_Print(
        line
    );
}


/* ============================================================
   MICROSECOND DELAY
   ============================================================ */

void delay_us(uint16_t us)
{
    __HAL_TIM_SET_COUNTER(&htim3, 0);

    while (__HAL_TIM_GET_COUNTER(&htim3) < us)
    {
    }
}


/* ============================================================
   ULTRASONIC DISTANCE
   ============================================================ */

uint32_t Ultrasonic_GetDistance(void)
{
    uint32_t pulseDuration;
    uint32_t timeout;

    HAL_GPIO_WritePin(TRIG_GPIO_Port, TRIG_Pin, GPIO_PIN_RESET);
    delay_us(2);

    HAL_GPIO_WritePin(TRIG_GPIO_Port, TRIG_Pin, GPIO_PIN_SET);
    delay_us(10);

    HAL_GPIO_WritePin(TRIG_GPIO_Port, TRIG_Pin, GPIO_PIN_RESET);

    __HAL_TIM_SET_COUNTER(&htim3, 0);

    while (HAL_GPIO_ReadPin(ECHO_GPIO_Port, ECHO_Pin) == GPIO_PIN_RESET)
    {
        timeout = __HAL_TIM_GET_COUNTER(&htim3);

        if (timeout > 30000)
        {
            return 999;
        }
    }

    __HAL_TIM_SET_COUNTER(&htim3, 0);

    while (HAL_GPIO_ReadPin(ECHO_GPIO_Port, ECHO_Pin) == GPIO_PIN_SET)
    {
        timeout = __HAL_TIM_GET_COUNTER(&htim3);

        if (timeout > 30000)
        {
            return 999;
        }
    }

    pulseDuration = __HAL_TIM_GET_COUNTER(&htim3);

    return pulseDuration / 58;
}


/* ============================================================
   UPDATE SLOT 2 INTERNALLY
   ============================================================ */

void UpdateUltrasonicSlot(void)
{
    uint32_t distance;

    if ((HAL_GetTick() - last_ultrasonic_time) < 100)
    {
        return;
    }

    last_ultrasonic_time = HAL_GetTick();

    distance = Ultrasonic_GetDistance();

    if (distance == 999)
    {
        slot2_distance = 999;
        return;
    }

    slot2_distance = distance;

    if (slot2_occupied == 0)
    {
        if (distance <= ULTRASONIC_OCCUPIED_CM)
        {
            slot2_occupied = 1;
        }
    }
    else
    {
        if (distance >= ULTRASONIC_FREE_CM)
        {
            slot2_occupied = 0;
        }
    }
}


/* ============================================================
   SLOT 1 - IR
   ============================================================ */

uint8_t Slot1_Occupied(void)
{
    if (HAL_GPIO_ReadPin(IR_GPIO_Port, IR_Pin) == IR_DETECTED_STATE)
    {
        return 1;
    }

    return 0;
}


/* ============================================================
   SLOT 2 - ULTRASONIC
   ============================================================ */

uint8_t Slot2_Occupied(void)
{
    return slot2_occupied;
}


/* ============================================================
   RFID SELECT
   ============================================================ */

static void RFID_Select(void)
{
    HAL_GPIO_WritePin(RFID_CS_GPIO_Port, RFID_CS_Pin, GPIO_PIN_RESET);
}

static void RFID_Deselect(void)
{
    HAL_GPIO_WritePin(RFID_CS_GPIO_Port, RFID_CS_Pin, GPIO_PIN_SET);
}


/* ============================================================
   RFID WRITE / READ REGISTER
   ============================================================ */

void RFID_WriteRegister(uint8_t reg, uint8_t value)
{
    uint8_t data[2];

    data[0] = (reg << 1) & 0x7E;
    data[1] = value;

    RFID_Select();
    HAL_SPI_Transmit(&hspi1, data, 2, HAL_MAX_DELAY);
    RFID_Deselect();
}

uint8_t RFID_ReadRegister(uint8_t reg)
{
    uint8_t tx[2];
    uint8_t rx[2];

    tx[0] = ((reg << 1) & 0x7E) | 0x80;
    tx[1] = 0x00;

    rx[0] = 0x00;
    rx[1] = 0x00;

    RFID_Select();
    HAL_SPI_TransmitReceive(&hspi1, tx, rx, 2, HAL_MAX_DELAY);
    RFID_Deselect();

    return rx[1];
}


/* ============================================================
   SET / CLEAR BIT MASK
   ============================================================ */

void RFID_SetBitMask(uint8_t reg, uint8_t mask)
{
    uint8_t value = RFID_ReadRegister(reg);
    RFID_WriteRegister(reg, value | mask);
}

void RFID_ClearBitMask(uint8_t reg, uint8_t mask)
{
    uint8_t value = RFID_ReadRegister(reg);
    RFID_WriteRegister(reg, value & (~mask));
}


/* ============================================================
   RFID RESET
   ============================================================ */

void RFID_Reset(void)
{
    HAL_GPIO_WritePin(RFID_RST_GPIO_Port, RFID_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(RFID_RST_GPIO_Port, RFID_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(50);
}


/* ============================================================
   RFID ANTENNA ON
   ============================================================ */

void RFID_AntennaOn(void)
{
    uint8_t value = RFID_ReadRegister(TxControlReg);

    if ((value & 0x03) != 0x03)
    {
        RFID_SetBitMask(TxControlReg, 0x03);
    }
}


/* ============================================================
   RFID INIT
   ============================================================ */

void RFID_Init(void)
{
    RFID_Reset();

    RFID_WriteRegister(TModeReg, 0x8D);
    RFID_WriteRegister(TPrescalerReg, 0x3E);
    RFID_WriteRegister(TReloadRegL, 30);
    RFID_WriteRegister(TReloadRegH, 0);
    RFID_WriteRegister(TxASKReg, 0x40);
    RFID_WriteRegister(ModeReg, 0x3D);

    RFID_AntennaOn();
}


/* ============================================================
   RFID COMMUNICATION
   ============================================================ */

uint8_t RFID_ToCard(uint8_t command, uint8_t *sendData, uint8_t sendLen, uint8_t *backData, uint16_t *backLen)
{
    uint8_t status = MI_ERR;
    uint8_t irqEn = 0;
    uint8_t waitIRQ = 0;
    uint8_t irqValue;
    uint8_t lastBits;
    uint8_t n;
    uint16_t timeout;
    uint16_t i;

    if (command == PCD_AUTHENT)
    {
        irqEn = 0x12;
        waitIRQ = 0x10;
    }

    if (command == PCD_TRANSCEIVE)
    {
        irqEn = 0x77;
        waitIRQ = 0x30;
    }

    RFID_WriteRegister(CommIEnReg, irqEn | 0x80);
    RFID_ClearBitMask(CommIrqReg, 0x80);
    RFID_SetBitMask(FIFOLevelReg, 0x80);
    RFID_WriteRegister(CommandReg, PCD_IDLE);

    for (i = 0; i < sendLen; i++)
    {
        RFID_WriteRegister(FIFODataReg, sendData[i]);
    }

    RFID_WriteRegister(CommandReg, command);

    if (command == PCD_TRANSCEIVE)
    {
        RFID_SetBitMask(BitFramingReg, 0x80);
    }

    timeout = 3000;

    do
    {
        irqValue = RFID_ReadRegister(CommIrqReg);
        timeout--;
    }
    while (timeout != 0 && !(irqValue & 0x01) && !(irqValue & waitIRQ));

    RFID_ClearBitMask(BitFramingReg, 0x80);

    if (timeout == 0)
    {
        return MI_ERR;
    }

    if (RFID_ReadRegister(ErrorReg) & 0x1B)
    {
        return MI_ERR;
    }

    status = MI_OK;

    if (irqValue & 0x01)
    {
        status = MI_NOTAGERR;
    }

    if (command == PCD_TRANSCEIVE)
    {
        n = RFID_ReadRegister(FIFOLevelReg);
        lastBits = RFID_ReadRegister(ControlReg) & 0x07;

        if (lastBits != 0)
        {
            *backLen = (n - 1) * 8 + lastBits;
        }
        else
        {
            *backLen = n * 8;
        }

        if (n == 0)
        {
            n = 1;
        }

        if (n > 16)
        {
            n = 16;
        }

        for (i = 0; i < n; i++)
        {
            backData[i] = RFID_ReadRegister(FIFODataReg);
        }
    }

    return status;
}


/* ============================================================
   RFID REQUEST
   ============================================================ */

uint8_t RFID_Request(uint8_t reqMode, uint8_t *tagType)
{
    uint8_t status;
    uint16_t backBits;

    RFID_WriteRegister(BitFramingReg, 0x07);

    tagType[0] = reqMode;

    status = RFID_ToCard(PCD_TRANSCEIVE, tagType, 1, tagType, &backBits);

    if (status != MI_OK || backBits != 0x10)
    {
        status = MI_ERR;
    }

    return status;
}


/* ============================================================
   READ UID
   ============================================================ */

uint8_t RFID_Anticoll(uint8_t *uid)
{
    uint8_t status;
    uint8_t checksum = 0;
    uint8_t i;
    uint16_t backBits;

    RFID_WriteRegister(BitFramingReg, 0x00);

    uid[0] = PICC_ANTICOLL;
    uid[1] = 0x20;

    status = RFID_ToCard(PCD_TRANSCEIVE, uid, 2, uid, &backBits);

    if (status == MI_OK)
    {
        for (i = 0; i < 4; i++)
        {
            checksum ^= uid[i];
        }

        if (checksum != uid[4])
        {
            status = MI_ERR;
        }
    }

    return status;
}


/* ============================================================
   RFID HALT
   ============================================================ */

void RFID_Halt(void)
{
    uint8_t buffer[4];
    uint16_t backBits;

    buffer[0] = PICC_HALT;
    buffer[1] = 0x00;

    RFID_ToCard(PCD_TRANSCEIVE, buffer, 2, buffer, &backBits);
}


/* ============================================================
   SLOT RESERVED
   ============================================================ */

uint8_t SlotReserved(uint8_t slot)
{
    if (DK_inside && DK_slot == slot)
    {
        return 1;
    }

    if (DEVA_inside && DEVA_slot == slot)
    {
        return 1;
    }

    return 0;
}


/* ============================================================
   FIND AVAILABLE SLOT
   ============================================================ */

uint8_t FindAvailableSlot(void)
{
    if (Slot1_Occupied() == 0 && SlotReserved(1) == 0)
    {
        return 1;
    }

    if (Slot2_Occupied() == 0 && SlotReserved(2) == 0)
    {
        return 2;
    }

    return 0;
}


/* ============================================================
   PRINT PARKING STATUS
   NOW LOGS CSV LINES INSTEAD OF FREE TEXT
   ============================================================ */

void PrintParkingStatus(void)
{
    if (Slot1_Occupied() || SlotReserved(1))
    {
        UART_LogCSV("STATUS", "SLOT1", "", "FULL");
    }
    else
    {
        UART_LogCSV("STATUS", "SLOT1", "", "FREE");
    }

    if (Slot2_Occupied() || SlotReserved(2))
    {
        UART_LogCSV("STATUS", "SLOT2", "", "FULL");
    }
    else
    {
        UART_LogCSV("STATUS", "SLOT2", "", "FREE");
    }
}


/* ============================================================
   PROCESS RFID
   NOW LOGS CSV LINES INSTEAD OF FREE TEXT
   ============================================================ */

void ProcessCard(uint8_t *uid)
{
    uint8_t slot;
    char slotStr[4];
    char uidStr[16];

    sprintf(uidStr, "%02X%02X%02X%02X", uid[0], uid[1], uid[2], uid[3]);

    /* DK */
    if (uid[0] == 0x43 && uid[1] == 0x35 && uid[2] == 0x28 && uid[3] == 0x2A)
    {
        if (DK_inside == 0)
        {
            slot = FindAvailableSlot();

            if (slot == 0)
            {
                UART_LogCSV("ENTRY", "DK", "", "FULL");
                return;
            }

            DK_inside = 1;
            DK_slot = slot;

            sprintf(slotStr, "%d", slot);
            UART_LogCSV("ENTRY", "DK", slotStr, "OK");
        }
        else
        {
            sprintf(slotStr, "%d", DK_slot);
            UART_LogCSV("EXIT", "DK", slotStr, "OK");

            DK_inside = 0;
            DK_slot = 0;
        }

        return;
    }

    /* DEVA */
    if (uid[0] == 0x03 && uid[1] == 0x56 && uid[2] == 0x71 && uid[3] == 0x11)
    {
        if (DEVA_inside == 0)
        {
            slot = FindAvailableSlot();

            if (slot == 0)
            {
                UART_LogCSV("ENTRY", "DEVA", "", "FULL");
                return;
            }

            DEVA_inside = 1;
            DEVA_slot = slot;

            sprintf(slotStr, "%d", slot);
            UART_LogCSV("ENTRY", "DEVA", slotStr, "OK");
        }
        else
        {
            sprintf(slotStr, "%d", DEVA_slot);
            UART_LogCSV("EXIT", "DEVA", slotStr, "OK");

            DEVA_inside = 0;
            DEVA_slot = 0;
        }

        return;
    }

    /* UNKNOWN */
    UART_LogCSV("ACCESS", uidStr, "", "DENIED");
}


/* ============================================================
   MAIN
   ============================================================ */

int main(void)
{
    uint8_t status;
    uint8_t tagType[2];
    uint8_t uid[5];
    uint8_t version;
    char message[80];

    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_SPI1_Init();
    MX_USART1_UART_Init();
    MX_TIM3_Init();

    HAL_TIM_Base_Start(&htim3);

    HAL_GPIO_WritePin(RFID_CS_GPIO_Port, RFID_CS_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(RFID_RST_GPIO_Port, RFID_RST_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(TRIG_GPIO_Port, TRIG_Pin, GPIO_PIN_RESET);

    HAL_Delay(100);

    UART_Print("\r\n================================\r\n");
    UART_Print("     SMART PARKING SYSTEM\r\n");
    UART_Print("================================\r\n");

    RFID_Init();

    version = RFID_ReadRegister(VersionReg);

    sprintf(message, "RC522 VERSION : 0x%02X\r\n", version);
    UART_Print(message);

    if (version == 0x91 || version == 0x92)
    {
        UART_LogCSV("SYSTEM", "RFID", "", "OK");
    }
    else
    {
        UART_LogCSV("SYSTEM", "RFID", "", "ERROR");
    }

    HAL_Delay(100);
    UpdateUltrasonicSlot();

    PrintParkingStatus();

    UART_Print("WAITING FOR RFID...\r\n");

    while (1)
    {
        UpdateUltrasonicSlot();

        status = RFID_Request(PICC_REQIDL, tagType);

        if (status == MI_OK)
        {
            status = RFID_Anticoll(uid);

            if (status == MI_OK)
            {
                UART_LogCSV("CARD", "", "", "DETECTED");

                ProcessCard(uid);

                PrintParkingStatus();

                UART_Print("WAITING FOR RFID...\r\n");
            }

            RFID_Halt();

            HAL_Delay(1500);
        }

        HAL_Delay(50);
    }
}


/* ============================================================
   SYSTEM CLOCK
   HSI = 8 MHz
   ============================================================ */

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
    {
        Error_Handler();
    }
}


/* ============================================================
   ERROR HANDLER
   ============================================================ */

void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}
