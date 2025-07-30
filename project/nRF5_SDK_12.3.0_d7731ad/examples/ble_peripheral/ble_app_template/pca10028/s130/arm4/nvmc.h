#ifndef NVMC_DRIVER_H
#define NVMC_DRIVER_H

#include <stdint.h>

/* NVMC driver return codes
 * - Indicate success or specific errors for debugging
 * - Used to handle invalid inputs or NVMC busy states
 * - Reference: General programming practice, not specific to NVMC documentation
 */
typedef enum {
    NVMC_SUCCESS = 0,          // Operation completed successfully
    NVMC_ERROR_INVALID_ADDR,   // Address is outside valid flash range
    NVMC_ERROR_BUSY,           // NVMC is busy with an ongoing operation
    NVMC_ERROR_ALIGNMENT       // Address or size is not properly aligned
} nvmc_result_t;
/*NVMC SET FLASH CONSTANTS*/
#define FLASH_PAGE_SIZE 1024 // 1KB IS THE PAGE SIZE FOR THE NRF MCU
#define FLASH_WORD_SIZE 4 // 32MCU SO ITS A 32-BIT WORD, 4BYTES

/* Initialize the NVMC
 * - Sets NVMC to read-only mode for safety
 * - Ensures no accidental writes or erases
 * - Reference: Section 6.1, CONFIG register must be set to Ren (read-only) initially
 */
void nvmc_init(void);

/* Erase a 1 KB page in Code region
 * - page_addr: Address of the page to erase (must be 4 KB aligned)
 * - Uses ERASEPAGE register for simplicity (works in Code region 1)
 * - Reference: Section 6.1.4, Table 7 (ERASEPAGE)
 */
nvmc_result_t nvmc_erase_page(uint32_t page_addr);

/* Write a buffer to flash
 * - addr: Destination flash address (must be 4-byte aligned)
 * - data: Array of 32-bit words to write
 * - size: Size in bytes (must be multiple of 4)
 * - Reference: Section 6.1.1, only word-aligned writes allowed
 */
nvmc_result_t nvmc_write_buffer(uint32_t addr, const uint32_t *data, uint32_t size);


#endif // NVMC_DRIVER_H