/*
 * File Name: nvmm.h
 * Author: PROJECTSUGAR
 * Date: 25Oct- 2017.
 * Description: 
 * Non-volatile (Flash) Memory Middleware
 * NVMM is kind of middleware, to drive the flash and make it easier to read and write.
 * The application layer need to assign 2flash pages and will get 1page nvmm back.
 * Note. The nvmm don't support multi- thread operation for now. 
 * 		Use nvmm in OS- free or in single- thread/ task please.
 */
#ifndef __NVMM_H__
#define __NVMM_H__

#include <stdint.h>
#include <stdlib.h>






/*
 * read non-volatile memory BYTES function type.
 */
typedef int (* read_nvbytes_t)(uint32_t address, uint8_t* buf, size_t bufsize, size_t datlen) ;
/*
 * write non-volatile memory WORDS function type.
 */
typedef int (* write_nvwords_t)(uint32_t address, uint8_t* dat, size_t wordnum) ;
/*
 * erase non-volatile page function type.
 */
typedef int (* erase_nvpage_t)(uint32_t address) ;

/*
 * initialize nvmm
 * need to call this method before using nvmm.
 * param read, write, erase are all flash operating methods. 
 * 		NVMM didn't realize them since the methods are platform dependent.
 *		You need to realize the 3methods in your Application layer or HAL.
 * 		The addresses in the 3methods are referring to base address 0, you should add the FLASH CHIP ADDRESS in your methods.
 * param flash_page_a and flash_page_b are the 2flash pages you assigned for the NVMM.
 * 		The parameters are flash page index, not the physical address of the flash memory.
 * 		For example, page0 means the 1st page in flash memory(Physical Address might be 0x00000000 + FLASH CHIP ADDRESS), 
 * 					suppose 2K bytes per page,
 *					page1 means the 2nd page in flash memory(Physical Address might be 0x00000800 + FLASH CHIP ADDRESS).
 *		Use 0xFFFF to leave the nvmm to use the default(PAGE A using page1 while PAGE B using page2).
 *		NOTE. You need to remove the pages assigned for nvmm from your compiler configuration, 
 *			or else, nvmm will confilits with your firmware image.
 * param flash_page_size is the page size of your flash, normally it based on the flash type you are using.
 *		Use 0xFFFF to leave the nvmm to use the default(2K bytes per page).
 * will return 0 for success executed, -1 for something error.
 */
int g_init_nvmm(read_nvbytes_t read, write_nvwords_t write, erase_nvpage_t erase, \
	uint16_t flash_page_a, uint16_t flash_page_b, uint16_t flash_page_size) ;

/*
 * write NVMM
 * You need to specify an id, all read and write are based on the id later.
 * return 0 if executed succeed.
 */
int g_write_nvmm(uint16_t id, size_t len, void* dat) ;


/*
 * read NVMM.
 * read NVMM item to specified buffer
 * return 0 if executed succeed.
 * will also return -1 if reading a no- written item.
 */
int g_read_nvmm(uint16_t id, size_t len, void *buf, size_t bufsize) ;

#endif /* NVMM_SNV.H */
