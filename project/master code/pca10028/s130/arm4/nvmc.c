#include "nvmc.h"

/*starting address of the nvmc peripheral*/
#define NVMC_BASE 0x4001E000

/*NVMC register definitions*/
#define NVMC_READY *((volatile uint32_t*)(NVMC_BASE +0x400)) // if 1==>ready;0==>busy
#define NVMC_CONFIG *((volatile uint32_t*)(NVMC_BASE + 0x504)) // to set the mode:read,write and erase
#define NVMC_ERASEPAGE *((volatile uint32_t*)(NVMC_BASE + 0x508)) // take an address of page to erase,after enabling the erase config and erasing a page not from the code may result an undesired behavior

/*NVMC CONFIG MODES*/
#define NVMC_CONFIG_READ 0x0 //read only 
#define NVMC_CONFIG_WRITE 0x01 //write mode, enable flash wwrites.
#define NVMC_CONFIG_ERASEPAGE 0x2 //erase mode , page erase form flash

/*NVMC SET FLASH CONSTANTS*/
#define FLASH_PAGE_SIZE 1024 // 1KB IS THE PAGE SIZE FOR THE NRF MCU
#define FLASH_WORD_SIZE 4 // 32MCU SO ITS A 32-BIT WORD, 4BYTES

/*NVMC READY CHECK + TIMEOUT:
-poll rethe ready reduster until the nvmc is not busy
-timeout fro 100000
*/
static nvmc_result_t nvmc_wait_ready(void){
		uint32_t timeout=100000;
		while(*(volatile uint32_t *)NVMC_READY==0){
		
		if(--timeout==0){
			return NVMC_ERROR_BUSY;}}
			return NVMC_SUCCESS;
}

/*INITIALIZE THE NVMC
-SETS CONFIG TO READ MODE
-WAIT FOR NVMC TO BE READY 
*/

void nvmc_init(void){
	NVMC_CONFIG=NVMC_CONFIG_READ;
	nvmc_wait_ready();
}
	
	/*ERASING A PAGE :1KB OF DATA:
			-we check if the enterd address is a multiple of 4kb and in our flash addresses ranges(in our case 256kb)
			-	using region 1 */
nvmc_result_t nvmc_erase_page	(uint32_t page_add){
		if(page_add % FLASH_PAGE_SIZE  !=0){
			return NVMC_ERROR_ALIGNMENT;}
		if(page_add >= 0x0001E590 && page_add <= 0x0003C000){
			return NVMC_ERROR_INVALID_ADDR;}
		
			nvmc_result_t result = nvmc_wait_ready();//wait untill ready
			if(result!=NVMC_SUCCESS){
				return result;
			}
			NVMC_CONFIG=NVMC_CONFIG_ERASEPAGE;//set erase mode
			NVMC_ERASEPAGE=page_add;//deleting the mentionned page
			result=nvmc_wait_ready();
			NVMC_CONFIG=NVMC_CONFIG_READ;
			return result;
}

/*writing a buffer to flash:
-addr:destination flash address
-size:size in byte(must be multiple of 4)
-set config on write mode than back to read mode
*/

nvmc_result_t nvmc_write_buffer(uint32_t addr,const uint32_t *data,uint32_t size){
if ((addr >= 0x0001E590 && addr <= 0x0003C000) || (addr + size) >= 0x0003C000) {
	return NVMC_ERROR_INVALID_ADDR;
	}
if (size % FLASH_WORD_SIZE != 0) {
    return NVMC_ERROR_ALIGNMENT;
}
if (addr % FLASH_WORD_SIZE != 0) {
    return NVMC_ERROR_ALIGNMENT;
}

	nvmc_result_t result=nvmc_wait_ready();
	if(result!=NVMC_SUCCESS){
		return result;
	}
	NVMC_CONFIG=NVMC_CONFIG_WRITE;//set write mode 
	/*writing data word by word
	-flash writes are 4bytes at a time 
	-cast addres for word alignement access
	*/
	uint32_t *dest=(uint32_t *)addr;//addr is the start address
	
	/*writing one word to flash */
	for (uint32_t i=0;i< size/FLASH_WORD_SIZE;i++){
		*dest++=data[i];//write one word and move pointer to next word
		result=nvmc_wait_ready();//wait untill done
		if(result!=NVMC_SUCCESS){
			NVMC_CONFIG=NVMC_CONFIG_READ;//back to normal mode
			return result;
		}
	}
	NVMC_CONFIG=NVMC_CONFIG_READ;
	return NVMC_SUCCESS;
}






