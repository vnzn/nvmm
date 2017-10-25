/*
 * File Name: nvmm.c
 * Author: PROJECTSUGAR
 * Date: 25Oct- 2017.
 * Description: 
 * Non-volatile (Flash) Memory Middleware
 * NVMM is kind of middleware, to drive the flash and make it easier to read and write.
 * The application layer need to assign 2flash pages and will get 1page nvmm back.
 * Note. The nvmm don't support multi- thread operation for now. 
 * 		Use nvmm in OS- free or in single- thread/ task please.
    Copyright 2017 PROJECTSUGAR

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
 */
#include "nvmm.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>



#define NVMM_PAGE_NULL       				0

#define NVMM_ACTIVE_PAGE_STATE			0xAAAAAAAA

#define NVMM_DUMMY_PAGE_STATE				0x00000000


#define NVMM_PAGE_SIZE_DEFAULT			2048				//default set to 2K, should adjust based on the platform you are using.
#define NVMM_PAGE_A_ID_DEFAULT			1				//use the 8th page as page A.
#define NVMM_PAGE_B_ID_DEFAULT       		2				//use the 9th page as page B.


#define NVMM_LINE_DELIMITER				0xAAAAAAAA
#define NVMM_LINE_MAXID					0x8000
#define NVMM_LINE_MAXLENGTH				0x8000

#define IS_LINEID_LEGAL(id)				(id < NVMM_LINE_MAXID)
#define IS_LINELENGTH_LEGAL(len)			(len < NVMM_LINE_MAXLENGTH)
#define IS_LINEDELIMITER_LEGAL(delimiter)	(delimiter == NVMM_LINE_DELIMITER)
#define PAD_LENGTH(len)					(((len + sizeof(uint32_t) - 1) / sizeof(uint32_t)) * sizeof(uint32_t))
#define FLASH_ADDRESS(pageid, offset)		((uint32_t )pageid * page_size + offset)

static read_nvbytes_t read_nvbytes = 0 ;
static write_nvwords_t write_nvwords = 0 ;
static erase_nvpage_t erase_nvpage = 0 ;

static uint16_t activedpage = 0xFFFF ;
static uint16_t ctindex = 0 ;	//content index.

static uint16_t page_a_id = NVMM_PAGE_A_ID_DEFAULT ;
static uint16_t page_b_id = NVMM_PAGE_B_ID_DEFAULT ;

static uint16_t page_size = NVMM_PAGE_SIZE_DEFAULT ;

/*
 * NVMM line header.
 */
typedef struct{
	uint16_t id ;
	uint16_t len ;
	uint32_t delimiter ;
}nvmm_lineheader_t ;




/*
 * NVMM page header.
 */
typedef struct{
	uint32_t state ;
	
	nvmm_lineheader_t dummy ;	//dummy line header, just put behind the page head id and store nothing.
}nvmm_pageheader_t ;




static void locate_ctindex(void)
{
	uint16_t index ;
	uint32_t delimiter ;
	
  
	for(index=page_size-sizeof(uint32_t);index>sizeof(nvmm_pageheader_t);index-=sizeof(uint32_t))
	{
		if(index == 8)
		{
			index = index ;
		}
		(* read_nvbytes)(FLASH_ADDRESS(activedpage, index), (uint8_t* )(&delimiter), \
						sizeof(uint32_t), sizeof(uint32_t)) ;
		if(IS_LINEDELIMITER_LEGAL(delimiter))
		{
			break;
		}
	}
	ctindex = index + sizeof(uint32_t) ;
}



/*
 *
 */
static int erase_page(uint16_t pageid)
{	
	uint8_t tmp ;
	uint16_t offset ;
	
	(* erase_nvpage)(FLASH_ADDRESS(pageid, 0));
	
	//check erase operation.
	for(offset=0;offset<page_size;offset++)
	{
		(* read_nvbytes)(FLASH_ADDRESS(pageid, offset), &tmp, sizeof(uint8_t), sizeof(uint8_t));
		if (tmp != 0xff)
		{
			return -1 ;
		}
	}
	
	
	
	return 0 ;
}


/*
 *
 */
static void clean_page(uint8_t pageid)
{
	uint8_t tmp ;
	uint16_t offset ;

	for(offset=0;offset<page_size;offset++)
	{
		(* read_nvbytes)(FLASH_ADDRESS(pageid, offset), &tmp, sizeof(uint8_t), sizeof(uint8_t));
		if (tmp != 0xff)
		{
			erase_page(pageid) ;
			return ;
		}
	}
}

/*
 *
 */
static int verify_words(uint16_t pageid, uint16_t offset, uint8_t* reference, size_t len)
{
	uint8_t tmp[4] ;

	while(len--)
	{
		(* read_nvbytes)(FLASH_ADDRESS(pageid, offset), tmp, sizeof(tmp), sizeof(tmp)) ;
		if (0 != memcmp(tmp, reference, sizeof(tmp)))
		{
			return -1 ;
		}
		offset += sizeof(tmp) ;
		reference += sizeof(tmp) ;
	}
	
	return 0 ;
}


/*
 *
 */
static void write_word(uint16_t pageid, uint16_t offset, uint8_t* word)
{
	(* write_nvwords)(FLASH_ADDRESS(pageid, offset), word, 1) ;
	verify_words(pageid, offset, word, 1) ;
}


static void write_words(uint16_t pageid, uint16_t offset, uint8_t* words, size_t count)
{
	(* write_nvwords)(FLASH_ADDRESS(pageid, offset), words, count) ;
	verify_words(pageid, offset, words, count) ;
}



static void active_page(uint16_t pageid)
{
	nvmm_pageheader_t header ;

	header.state = NVMM_ACTIVE_PAGE_STATE ;
	header.dummy.id = 0xCAFE ;
	header.dummy.delimiter = NVMM_LINE_DELIMITER ;
	write_words( pageid, 0, (uint8_t* )(&header), PAD_LENGTH(sizeof(nvmm_pageheader_t)) / sizeof(uint32_t)) ;
	activedpage = pageid ;
}

/*
 *
 */
static uint16_t find_line_address(uint16_t pageid, uint16_t offset, uint16_t lineid)
{
	nvmm_lineheader_t lheader ;
	
	offset -= sizeof(nvmm_lineheader_t) ;

	while(offset >= sizeof(nvmm_pageheader_t))
	{
		(* read_nvbytes)(FLASH_ADDRESS(pageid, offset), (uint8_t *)(&lheader), \
						sizeof(nvmm_lineheader_t), sizeof(nvmm_lineheader_t));

		if (lheader.id == lineid)
		{//found.
			return offset - lheader.len ;
		}
		else if (!IS_LINELENGTH_LEGAL(lheader.len))
		{
			offset -= sizeof(nvmm_lineheader_t) ;
		}
		else
		{
			if (lheader.len + sizeof(nvmm_lineheader_t) <= offset)
			{
				offset -= lheader.len + sizeof(nvmm_lineheader_t) ;
			}
			else
			{
				/*
				 * The content is incorrect, might be hardware fault.
				 * need to re- initialize current page here or use assert to mention.
				 *
				 */
				return 0 ;	
			}
		}
	}
	
	
	return 0;
}

static void copy_line(uint16_t pageid, uint16_t offset_tgt, size_t len, uint16_t offset_src)
{
	uint8_t tmp[4] ;
	size_t i = 0 ;

	// Copy over the data
	while (i < len)
	{
		(* read_nvbytes)(FLASH_ADDRESS(activedpage, offset_src + i), tmp, sizeof(tmp), sizeof(tmp)) ;
		write_word(pageid, offset_tgt + i, tmp);

		i += sizeof(tmp);
	}
}

static int defrag_page(uint16_t src_pageid)
{
	uint16_t offset_src ;
	uint16_t offset_tgt ;
	uint16_t tgt_pageid ;//target page id.
	nvmm_lineheader_t lheader ;
	uint16_t lineid_tmp = 0xFFFF ;//history line id.

	tgt_pageid = (src_pageid == page_a_id)? page_b_id : page_a_id ;

	offset_tgt = sizeof(nvmm_pageheader_t) ;

	offset_src = ctindex - sizeof(nvmm_lineheader_t) ;

  	while(offset_src >= sizeof(nvmm_pageheader_t))
	{
		(* read_nvbytes)(FLASH_ADDRESS(src_pageid, offset_src), (uint8_t* )(&lheader), \
					sizeof(nvmm_lineheader_t), sizeof(nvmm_lineheader_t));
		if(!IS_LINEID_LEGAL(lheader.id) || !(IS_LINEDELIMITER_LEGAL(lheader.delimiter)))
		{
			 if(!IS_LINELENGTH_LEGAL(lheader.len))
			 {
				offset_src -= sizeof(nvmm_lineheader_t) ;
			 }
			 else
			 {
				if(lheader.len + sizeof(nvmm_lineheader_t) <= offset_src)
				{
					offset_src -= lheader.len + sizeof(nvmm_lineheader_t) ;
				}
				else
				{
					/*
					 * The content is incorrect, might be hardware fault.
					 * need to re- initialize current page here or use assert to mention.
					 *
					 */
					return -1 ;	
				}
			 }

			 continue;
		}

		if(IS_LINEID_LEGAL(lheader.id) && lheader.id != lineid_tmp)
		{
			lineid_tmp = lheader.id ;

			if (find_line_address(tgt_pageid, offset_tgt, lineid_tmp) == 0)
			{//no exist line found. create a new one.
				copy_line(tgt_pageid, offset_tgt, lheader.len + sizeof(nvmm_lineheader_t), offset_src - lheader.len);

				offset_tgt += lheader.len + sizeof(nvmm_lineheader_t) ;
			}	
		}
		
		offset_src -= lheader.len + sizeof(nvmm_lineheader_t) ;
	}


	//active target page.
	active_page(tgt_pageid) ;

	ctindex = offset_tgt ;

	erase_page(src_pageid) ;
	
	
	return 0 ;
}


/*
 * check current nvmm and see current status of nvmm.
 * will return 0 for success, -1 for something error.
 *
 */
static int check_nvmm( void )
{
	nvmm_pageheader_t header ;
	uint16_t dummypage = NVMM_PAGE_NULL ;

	activedpage = NVMM_PAGE_NULL;

	//check page A.
	(* read_nvbytes)(FLASH_ADDRESS(page_a_id, 0), (uint8_t* )(&header), \
					sizeof(nvmm_pageheader_t), sizeof(nvmm_pageheader_t)) ;
	if ( header.state == NVMM_ACTIVE_PAGE_STATE)
	{//current page is used as actived page.
		if(activedpage == NVMM_PAGE_NULL)
		{
			activedpage = page_a_id ;
		}
	}
	else if(header.state == NVMM_DUMMY_PAGE_STATE)
	{//current page is used as dummy page.
		dummypage = page_a_id ;
	}
	else
	{//no defined page, format it.
		clean_page(page_a_id);
	}
	
	//check page B.
	(* read_nvbytes)(FLASH_ADDRESS(page_b_id, 0), (uint8_t* )(&header), \
					sizeof(nvmm_pageheader_t), sizeof(nvmm_pageheader_t)) ;
	if ( header.state == NVMM_ACTIVE_PAGE_STATE)
	{//current page is used as actived page.
		if(activedpage == NVMM_PAGE_NULL)
		{//good to go.
			activedpage = page_b_id ;
		}
		else
		{//found 2actived page, something wrong on last activating. 
			//format all.
			clean_page(activedpage);
			clean_page(page_b_id);
			activedpage = NVMM_PAGE_NULL;
		}
	}
	else if(header.state == NVMM_DUMMY_PAGE_STATE)
	{//current page is used as dummy page.
		dummypage = page_b_id ;
	}
	else
	{//no defined page, format it.
		clean_page(page_b_id);
	}

	if (activedpage == NVMM_PAGE_NULL)
	{//no page actived, normally the 1st time operating current flash.
		if (dummypage == NVMM_PAGE_NULL)
		{//good to go.
			active_page(page_a_id) ;
			ctindex = sizeof(nvmm_pageheader_t) ;

			return 0 ;
		}
		else
		{//last operating hasn't done, complete it now.
			//copying also hasn't done yet.
			activedpage = dummypage ;
			locate_ctindex() ;

			defrag_page(dummypage) ;
		}
	}
	else
	{//
		if (dummypage != NVMM_PAGE_NULL)
		{//last operating hasn't done, complete it now.
			//copying already done. Just do erase on dummy page.
			erase_page(dummypage) ;
		}

		
		//re-locate the content index.
		locate_ctindex() ;
	}

  return 0 ;
}


static void dummy_activedpage(void)
{
	nvmm_pageheader_t header ;

	header.state = NVMM_DUMMY_PAGE_STATE;

	write_word(activedpage, 0, (uint8_t* )(&header.state));
}


static void write_line(uint16_t pageid, uint16_t offset, uint16_t lineid, uint16_t len, uint8_t* dat)
{
	nvmm_lineheader_t header ;

	header.id = 0xFFFF ;
	header.len = len ;
	header.delimiter = 0xFFFFFFFF ;

	write_words(pageid, offset + len, (uint8_t *)(&header), \
				PAD_LENGTH(sizeof(nvmm_lineheader_t)) / sizeof(uint32_t));
	write_words(pageid, offset, dat, len / sizeof(uint32_t));


	header.id = lineid ;
	write_words(pageid, offset + len, (uint8_t *)(&header), \
				PAD_LENGTH(sizeof(nvmm_lineheader_t)) / sizeof(uint32_t));


	header.delimiter = NVMM_LINE_DELIMITER ;
	write_words(pageid, offset + len, (uint8_t *)(&header), \
				PAD_LENGTH(sizeof(nvmm_lineheader_t)) / sizeof(uint32_t));
}



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
	uint16_t flash_page_a, uint16_t flash_page_b, uint16_t flash_page_size) 
{
	if(read == 0 || write == 0 || erase == 0)
	{
		return -1 ;
	}
	
	read_nvbytes = read ;
	write_nvwords = write ;
	erase_nvpage = erase ;
	if(flash_page_a != 0xFFFF)
	{
		page_a_id = flash_page_a ;
	}
	if(flash_page_b != 0xFFFF)
	{
		page_b_id = flash_page_b ;
	}	
	if(flash_page_size != 0xFFFF)
	{
		page_size = flash_page_size ;
	}	
	
	return check_nvmm() ;
}


/*
 * write NVMM
 * You need to specify an id, all read and write are based on the id later.
 * return 0 if executed succeed.
 */
int g_write_nvmm(uint16_t id, size_t len, void* dat)
{
	size_t padded_len ;
	uint16_t offset ;
	uint8_t tmp ;
	size_t i ;

	offset = find_line_address(activedpage, ctindex, id);

	if(offset > 0)
	{
		for(i = 0; i < len; i++)
		{
			(* read_nvbytes)(FLASH_ADDRESS(activedpage, offset), &tmp, sizeof(tmp), sizeof(tmp)) ;
			if(tmp != *((uint8_t *)dat + i))
			{
				break ;
			}
			offset++;
		}

		if (i == len)
		{//no change.
			return 0 ;
		}
	}

	padded_len = PAD_LENGTH(len) ;


	if(ctindex + padded_len + sizeof(nvmm_pageheader_t) > page_size)
	{
		dummy_activedpage() ;
		defrag_page(activedpage) ;
	}

  
	write_line(activedpage, ctindex, id, padded_len, dat) ;


	ctindex += padded_len + sizeof(nvmm_lineheader_t) ;


	return 0 ;
}



/*
 * read NVMM.
 * read NVMM item to specified buffer
 * return 0 if executed succeed.
 * will also return -1 if reading a no- written item.
 */
int g_read_nvmm(uint16_t id, size_t len, void *buf, size_t bufsize)
{
	uint16_t offset ;
	
	if(buf == 0 || len == 0 || bufsize == 0 || bufsize < len)
	{
		return -1 ;
	}
	
	offset = find_line_address(activedpage, ctindex, id) ;

	if (offset != 0)
	{
		(* read_nvbytes)(FLASH_ADDRESS(activedpage, offset), buf, bufsize, len) ;
		return 0 ;
	}
	return -1 ;
}

