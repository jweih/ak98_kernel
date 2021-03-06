/**
 * @filename wrap_nand.c
 * @brief AK880x nandflash driver
 *
 * This file wrap the nand flash driver in nand_control.c file.
 * Copyright (C) 2010 Anyka (GuangZhou) Software Technology Co., Ltd.
 * @author zhangzheng
 * @modify 
 * @date 2010-10-10
 * @version 1.0
 * @ref
 */

#include <linux/list.h>
#include <linux/init.h>
#include <linux/mtd/partitions.h>
#include <linux/math64.h>
#include <mtd/mtd-abi.h>
#include "anyka_cpu.h"
#include "nand_control.h"
#include "wrap_nand.h"

#define YAFFS_OOB_SIZE_LARGE            28
#define YAFFS_OOB_SIZE_LITTLE		8	//lgh

#define PASSWORD_ADDR_OFFSET	    (4)
#define PASSWORD_STR			    "ANYKA783"                 
#define PASSWORD_LENGTH			    (sizeof(PASSWORD_STR) - 1) 

#define ZZ_DEBUG                    0
#define NAND_BOOT_DATA_SIZE         472


extern int page_shift;
extern struct mtd_info *g_master;


T_Nandflash_Add g_sNF = {
							{0,0,0,0}, 
							3, 
							2, 
							NAND_8K_PAGE, 
							ECC_24BIT_P1KB, 
							8192, 
							128
					    };

/*
	T_Nandflash_Add g_sNF = {
								{0,0,0,0}, 
								3, 
								2, 
								NAND_2K_PAGE, 
								ECC_4BIT_P512B, 
								2048, 
								128
							};
*/
/*
	T_Nandflash_Add g_sNF = {
								{0,0,0,0}, 
								3, 
								1, 
								NAND_512B_PAGE, 
								ECC_4BIT_P512B, 
								512, 
								32
							};
*/


T_NAND_PHY_INFO *g_pNand_Phy_Info = NULL;

#ifdef CONFIG_MTD_NAND_TEST   
T_U32 *nand_erase_test = NULL;
T_U32 nand_total_blknum = 0;
#endif

#if 1
T_PNandflash_Add g_pNF = NULL;
#else
T_PNandflash_Add g_pNF = &g_sNF;
#endif 

T_PFHA_INIT_INFO g_pinit_info = NULL;
T_PFHA_LIB_CALLBACK g_pCallback = NULL;
T_U8 *nand_data_buf = NULL;

T_Nandflash_Add try_nf_add[] = 
{
    {{0,0,0,0}, 3, 2, NAND_8K_PAGE, ECC_24BIT_P1KB, 8192, 128},
	{{0,0,0,0}, 3, 2, NAND_4K_PAGE, ECC_8BIT_P512B, 4096, 128},
	{{0,0,0,0}, 2, 1, NAND_512B_PAGE, ECC_8BIT_P512B, 512, 128},
	{{0,0,0,0}, 2, 1, NAND_512B_PAGE, ECC_8BIT_P512B, 512, 128}
};

int init_fha_lib(void)
{
	int retval = 0;
	
	g_pinit_info = kmalloc(sizeof(T_FHA_INIT_INFO), GFP_KERNEL);
	if(g_pinit_info == NULL)
	{
		printk(KERN_INFO "zz wrap_nand.c init_fha_lib() 71 g_pinit_info==NULL error!!!\n");
		return FHA_FAIL;
	}

	g_pinit_info->nChipCnt = 1;
	g_pinit_info->nBlockStep = 1;
	g_pinit_info->eAKChip = FHA_CHIP_980X;
	g_pinit_info->ePlatform = PLAT_LINUX;
	g_pinit_info->eMedium = MEDIUM_NAND;
	g_pinit_info->eMode = 0;
			
	g_pCallback = kmalloc(sizeof(T_FHA_LIB_CALLBACK), GFP_KERNEL);
	if(g_pCallback == NULL)
	{
		printk(KERN_INFO "zz wrap_nand.c init_fha_lib() 85 g_pCallback==NULL\n");
		return FHA_FAIL;
	}
	g_pCallback->Erase = nand_erase_callback;
	g_pCallback->Write = (FHA_Write)nand_write_callback;
	g_pCallback->Read = (FHA_Read)nand_read_callback;
	g_pCallback->RamAlloc = ram_alloc;
	g_pCallback->RamFree = ram_free;
	g_pCallback->MemCmp = (FHA_MemCmp)memcmp;
	g_pCallback->MemSet = (FHA_MemSet)memset;
	g_pCallback->MemCpy = (FHA_MemCpy)memcpy;
	g_pCallback->Printf = (FHA_Printf)printk; //print_callback;
	//g_pCallback->Printf = (FHA_Printf)printk_my; //print_callback;		//lgh
	g_pCallback->ReadNandBytes = nand_read_bytes_callback;

	#ifndef CONFIG_MTD_DOWNLOAD_MODE
	#if ZZ_DEBUG
	printk(KERN_INFO "zz wrap_nand.c init_fha_lib() 101 before FHA_mount()\n");
	#endif
	
	retval = FHA_mount(g_pinit_info, g_pCallback, AK_NULL);

	if (retval)
	{
		
		if(init_globe_para() == AK_FALSE)
		{
            printk(KERN_INFO " init_fha_lib() init_globe_para!!!\n");
		    return FHA_FAIL;
		}
	}
	else
	{
		printk(KERN_INFO "zz wrap_nand.c init_fha_lib() FHA_mount return:%d error!!!\n", retval);
		return FHA_FAIL;
	}
	
	FHA_asa_scan(AK_TRUE);
	#else
	if(g_pNand_Phy_Info == NULL)
	{
		printk(KERN_NOTICE "zz wrap_nand.c init_fha_lib() 120 g_pNand_Phy_Info==NULL error!!!\n");
		return FHA_FAIL;
	}
	
	retval =  FHA_burn_init(g_pinit_info, g_pCallback, g_pNand_Phy_Info);
	FHA_asa_scan(AK_TRUE);
	#endif

	if(retval == FHA_FAIL)
	{
		kfree(g_pinit_info);
		kfree(g_pCallback);
		g_pinit_info = NULL;
		g_pCallback = NULL;
		printk(KERN_INFO "zz wrap_nand.c init_fha_lib() 134 failed!!!\n");
	}	

    nand_data_buf = kmalloc(g_pNF->PageSize, GFP_KERNEL);
    if(nand_data_buf == NULL)
    {
        printk(KERN_NOTICE "nand_data_buf malloc fail!!!\n");
        return FHA_FAIL;
    }
    
	return retval;
}


void Nand_Config_Data_Spare(T_PNAND_ECC_STRU data_ctrl, T_U8* data, T_U32 data_len, ECC_TYPE ecc_type,
							    T_PNAND_ECC_STRU spare_ctrl, T_U8* spare, T_U32 spare_len)
{
   	data_ctrl->buf = data;   
    	data_ctrl->buf_len = data_len;
    	data_ctrl->ecc_type = ecc_type;		
		
    	spare_ctrl->buf = spare;
    	spare_ctrl->buf_len= spare_len;
    	spare_ctrl->ecc_type = (ecc_type > ECC_12BIT_P512B) ? ECC_12BIT_P512B : ecc_type;	
		
	if(g_pNF->PageSize < NAND_PAGE_SIZE_2KB){			
		//printk(KERN_INFO "%s:line %d: brached to 512Byte/page\n", __func__, __LINE__);
		if(spare_len > 9)	
			printk(KERN_ERR "%s:%d:spare length is %lu! It should be less @@9Bytes@@\n", __func__, __LINE__, spare_len);		
	    	data_ctrl->ecc_section_len =  data_len +  spare_len;
	    	spare_ctrl->ecc_section_len = data_len + spare_len;			
	}
	else{	
	    	data_ctrl->ecc_section_len = (ecc_type > ECC_12BIT_P512B) ? NAND_DATA_SIZE_P1KB : NAND_DATA_SIZE_P512B;
    		spare_ctrl->ecc_section_len = spare_len;			
	}
	/*
	printk(KERN_INFO "data_ctrl: buf=%p, buf_len=%lu, ecc_section_len=%lu, ecc_type=%d\n", \
		data_ctrl->buf, data_ctrl->buf_len, data_ctrl->ecc_section_len, data_ctrl->ecc_type);
   	printk(KERN_INFO "spare_ctrl: buf=%p, buf_len=%lu, ecc_section_len=%lu, ecc_type=%d\n", \
		spare_ctrl->buf, spare_ctrl->buf_len, spare_ctrl->ecc_section_len, spare_ctrl->ecc_type);	
	*/

} 

void ak_nand_write_buf(struct mtd_info *mtd, const u_char * buf, int len)
{
    T_U32 chip_num = 0;
    T_U32 row_addr = page_shift; 
    T_U32 clm_addr = 0;
    struct nand_chip *chip = mtd->priv;
    T_NAND_ECC_STRU data_ctrl;
    T_NAND_ECC_STRU spare_ctrl;
	
	if(g_pNF == NULL)
	{
		printk(KERN_NOTICE "zz wrap_nand.c ak_nand_write_buf() 176 g_pNF==NULL error!!!\n");
		return;
	}
    	//Nand_Config_Data(&data_ctrl, buf, g_pNF->PageSize, g_pNF->EccType);
	//Nand_Config_Spare(&spare_ctrl, chip->oob_poi + 2, YAFFS_OOB_SIZE, g_pNF->EccType);
	if(g_pNF->PageSize < NAND_PAGE_SIZE_2KB)
		Nand_Config_Data_Spare(	&data_ctrl, (T_U8 *)buf, g_pNF->PageSize, g_pNF->EccType,\
								&spare_ctrl, chip->oob_poi + 8, YAFFS_OOB_SIZE_LITTLE);
	else
		Nand_Config_Data_Spare(	&data_ctrl, (T_U8 *)buf, g_pNF->PageSize, g_pNF->EccType,\
								&spare_ctrl, chip->oob_poi + 2, YAFFS_OOB_SIZE_LARGE);
	nand_writepage_ecc(chip_num, row_addr, clm_addr, g_pNF, &data_ctrl, &spare_ctrl);  
}

void ak_nand_read_buf(struct mtd_info *mtd, u_char *buf, int len)
{
    T_U32 chip_num = 0;
    T_U32 row_addr = page_shift; 
    T_U32 clm_addr = 0;
    struct nand_chip *chip = mtd->priv;
    T_NAND_ECC_STRU data_ctrl;
    T_NAND_ECC_STRU spare_ctrl;
	
    if(g_pNF == NULL)
	{
		printk(KERN_NOTICE "zz wrap_nand.c ak_nand_read_buf() 195 g_pNF==NULL error!!!\n");
		return;
	}
    	//Nand_Config_Data(&data_ctrl, buf, g_pNF->PageSize, g_pNF->EccType);
	//Nand_Config_Spare(&spare_ctrl, chip->oob_poi + 2, YAFFS_OOB_SIZE, g_pNF->EccType);
	if(g_pNF->PageSize < NAND_PAGE_SIZE_2KB)
		Nand_Config_Data_Spare(	&data_ctrl, buf, g_pNF->PageSize, g_pNF->EccType,\
								&spare_ctrl, chip->oob_poi + 8, YAFFS_OOB_SIZE_LITTLE);
	else
		Nand_Config_Data_Spare(	&data_ctrl, buf, g_pNF->PageSize, g_pNF->EccType,\
								&spare_ctrl, chip->oob_poi + 2, YAFFS_OOB_SIZE_LARGE);	
    nand_readpage_ecc(chip_num, row_addr, clm_addr, g_pNF, &data_ctrl, &spare_ctrl);   
}

void ak_nand_select_chip(struct mtd_info *mtd, int chip)
{

}

static void ak_nand_config_timeseq(unsigned long cmd_len, unsigned long data_len)
{
    nand_settiming(cmd_len, data_len);    
}
u_char ak_nand_read_byte(struct mtd_info *mtd)
{
    T_U32 retval = 0;
    return (u_char)retval;
}

int ak_nand_scan_bbt(struct mtd_info *mtd)
{
    /*because FHA_asa_scan has built the bbt ,so we should not to do anything*/
    return 1; 
}


int ak_nand_init_size(struct mtd_info *mtd, struct nand_chip *chip,
			u8 *id_data)
{
    int busw;

    if (g_pNand_Phy_Info != AK_NULL)
    {
        mtd->writesize = g_pNand_Phy_Info->page_size;                                
        mtd->erasesize = g_pNand_Phy_Info->page_size * g_pNand_Phy_Info->page_per_blk; 
    }
    else
    {
        printk("g_pNand_Phy_Info is NULL, init size fail!\n");
        BUG();
    }
    
    mtd->oobsize = 128;     //only save yaffs2 filesystem information.
    busw = (chip->options & (1 << 1)) ? NAND_BUSWIDTH_16 : 0;
   // printk("hy write=%u,erasesize =%u, busw=%d####\n",mtd->writesize,mtd->erasesize,busw);
    return busw;
}

int ak_nand_read_page_hwecc(struct mtd_info *mtd, struct nand_chip *chip,
				                   uint8_t *buf, int page)
{
    T_U32 chip_num = 0;
    T_U32 row_addr = page; 
    T_U32 clm_addr = 0;
    T_NAND_ECC_STRU data_ctrl;
    T_NAND_ECC_STRU spare_ctrl;
    if(g_pNF == NULL)
	{
		printk(KERN_NOTICE "zz wrap_nand.c ak_nand_read_page_hwecc() 224 g_pNF==NULL error!!!\n");
		return 1;
	} 
 	//Nand_Config_Data(&data_ctrl, buf, g_pNF->PageSize, g_pNF->EccType);
	//Nand_Config_Spare(&spare_ctrl, chip->oob_poi + 2, YAFFS_OOB_SIZE, g_pNF->EccType);
	if(g_pNF->PageSize < NAND_PAGE_SIZE_2KB)
		Nand_Config_Data_Spare(	&data_ctrl, buf, g_pNF->PageSize, g_pNF->EccType,\
								&spare_ctrl, chip->oob_poi + 8, YAFFS_OOB_SIZE_LITTLE);
	else 
		Nand_Config_Data_Spare(	&data_ctrl, buf, g_pNF->PageSize, g_pNF->EccType,\
								&spare_ctrl, chip->oob_poi + 2, YAFFS_OOB_SIZE_LARGE);
    return nand_readpage_ecc(chip_num, row_addr, clm_addr, g_pNF, &data_ctrl, &spare_ctrl);
}

void ak_nand_write_page_hwecc(struct mtd_info *mtd, struct nand_chip *chip,
				                     const uint8_t *buf)
{
    T_U32 chip_num = 0;
    T_U32 row_addr = page_shift; 
    T_U32 clm_addr = 0;
    T_NAND_ECC_STRU data_ctrl;
    T_NAND_ECC_STRU spare_ctrl;
    if(g_pNF == NULL)
	{
		printk(KERN_NOTICE "zz wrap_nand.c ak_nand_write_page_hwecc() 242 g_pNF==NULL error\n");
		return;
	}
	//Nand_Config_Data(&data_ctrl, buf, g_pNF->PageSize, g_pNF->EccType);
	//Nand_Config_Spare(&spare_ctrl, chip->oob_poi + 2, YAFFS_OOB_SIZE, g_pNF->EccType);
	if(g_pNF->PageSize < NAND_PAGE_SIZE_2KB)
		Nand_Config_Data_Spare(	&data_ctrl, (T_U8 *)buf, g_pNF->PageSize, g_pNF->EccType,\
								&spare_ctrl, chip->oob_poi + 8, YAFFS_OOB_SIZE_LITTLE);
	else 
		Nand_Config_Data_Spare(	&data_ctrl, (T_U8 *)buf, g_pNF->PageSize, g_pNF->EccType,\
								&spare_ctrl, chip->oob_poi + 2, YAFFS_OOB_SIZE_LARGE);	


	nand_writepage_ecc(chip_num, row_addr, clm_addr, g_pNF, &data_ctrl, &spare_ctrl);
}

int ak_nand_read_page_raw(struct mtd_info *mtd, struct nand_chip *chip,
			                    uint8_t *buf, int pag)
{
	#if ZZ_DEBUG
	printk(KERN_INFO "zz wrap_nand.c ak_nand_read_page_raw() 254\n");
	#endif
    return 0;
}

void ak_nand_write_page_raw(struct mtd_info *mtd, struct nand_chip *chip,
				                  const uint8_t *buf)
{
	#if ZZ_DEBUG
	printk(KERN_INFO "zz wrap_nand.c ak_nand_write_page_raw() 263\n");
	#endif
}

int ak_nand_read_oob(struct mtd_info *mtd, struct nand_chip *chip,
			                   int page, int sndcmd)
{      
    T_U32 chip_num = 0;
    T_U32 row_addr = page; 
    T_U32 clm_addr = 0;
    T_NAND_ECC_STRU data_ctrl;
    T_NAND_ECC_STRU spare_ctrl;
	
    if(g_pNF == NULL)
	{
		printk(KERN_NOTICE "zz wrap_nand.c ak_nand_read_oob() 280 g_pNF==NULL error!!!\n");
		return 1;
	}
    if (sndcmd) 
    {
		chip->cmdfunc(mtd, 0x50, 0, page); //0x50 read oob cmd
		sndcmd = 0;
	}
	//Nand_Config_Data(&data_ctrl, oob_data_buf, g_pNF->PageSize, g_pNF->EccType);
	//Nand_Config_Spare(&spare_ctrl, chip->oob_poi + 2, YAFFS_OOB_SIZE, g_pNF->EccType);
		
	if(g_pNF->PageSize  < NAND_PAGE_SIZE_2KB)
		Nand_Config_Data_Spare(	&data_ctrl, nand_data_buf, g_pNF->PageSize, g_pNF->EccType,\
								&spare_ctrl, chip->oob_poi + 8, YAFFS_OOB_SIZE_LITTLE);
	else
		Nand_Config_Data_Spare(	&data_ctrl, nand_data_buf, g_pNF->PageSize, g_pNF->EccType,\
								&spare_ctrl, chip->oob_poi + 2, YAFFS_OOB_SIZE_LARGE);	
	
    nand_readpage_ecc(chip_num, row_addr, clm_addr, g_pNF, &data_ctrl, &spare_ctrl);   
	return sndcmd;
}

int ak_nand_write_oob(struct mtd_info *mtd, struct nand_chip *chip,
			                   int page)
{
    return 0;
}

T_U32 nand_erase_callback(T_U32 chip_num,  T_U32 startpage)
{
	int ret;

	if(g_pNF == NULL)
	{
		printk(KERN_NOTICE "zz wrap_nand.c nand_erase_callback() 304 g_pNF==NULL error\n");
		return 1;
	}

	ret = nand_eraseblock(chip_num, startpage, g_pNF);
	/* Chang the return value to adapt the fha library */
	if (ret == 0)
		return 1;
	else
		return 0;
}

T_U32 nand_read_callback(T_U32 chip_num, T_U32 page_num, T_U8 *data, 
           T_U32 data_len, T_U8 *oob, T_U32 oob_len, T_U32 eDataType)
{
    T_NAND_ECC_STRU data_ctrl;
    T_NAND_ECC_STRU spare_ctrl;
	int i = 0;
	T_U32 boot_page_size = 0;
	int try_count = sizeof(try_nf_add)/sizeof(try_nf_add[0]);

	if(eDataType == FHA_GET_NAND_PARAM)
	{
		#if ZZ_DEBUG
		printk(KERN_INFO "zz wrap_nand.c nand_read_callback() 321 eDataType == FHA_GET_NAND_PARAM\n");
		#endif
		for(i=0; i<try_count; i++)
		{	
			#if ZZ_DEBUG
			printk(KERN_INFO "i=%d row_cycle=%d col_cycle=%d chip_type=%d ecc_type=%d page_size=%lu PagesPerBlock=%lu\n",
			       i, try_nf_add[i].RowCycle, try_nf_add[i].ColCycle, try_nf_add[i].ChipType, try_nf_add[i].EccType,
			       try_nf_add[i].PageSize, try_nf_add[i].PagesPerBlock);
			#endif
			if(AK_TRUE== try_nand_para(chip_num, page_num, data, data_len, oob, oob_len, &try_nf_add[i]))
			{
				#if ZZ_DEBUG
				printk(KERN_INFO "zz wrap_nand.c nand_read_callback() 333 try nand para successed i=%d\n", i);
				#endif
				return FHA_SUCCESS;
			}
		}
		return FHA_FAIL;
				
	}
	
	if(g_pNF == NULL)
	{
		printk(KERN_NOTICE "zz wrap_nand.c nand_read_callback() 357 g_pNF==NULL error\n");
		return FHA_FAIL;
	}
		
    if(eDataType == FHA_DATA_BOOT)
    {
        boot_page_size = (data_len > 4096) ? 4096 : data_len;

        if (0 == page_num)
        {
            data_ctrl.buf = data;
        	data_ctrl.buf_len = NAND_BOOT_DATA_SIZE;
        	data_ctrl.ecc_section_len = NAND_BOOT_DATA_SIZE;
        	data_ctrl.ecc_type = ECC_32BIT_P1KB;                   
        }
    	else
    	{
            data_ctrl.buf = data;
            data_ctrl.buf_len = boot_page_size;
            data_ctrl.ecc_section_len = 512;
            data_ctrl.ecc_type = g_pNF->EccType;
    	}

        nand_readpage_ecc(0, page_num, 0, g_pNF, &data_ctrl, AK_NULL);        
    }
	else
	{
    		//Nand_Config_Data(&data_ctrl, data, data_len, g_pNF->EccType);
		//Nand_Config_Spare(&spare_ctrl, oob, oob_len, g_pNF->EccType);
		if(g_pNF->PageSize  < NAND_PAGE_SIZE_2KB)
			Nand_Config_Data_Spare(	&data_ctrl, data, data_len, g_pNF->EccType,\
									&spare_ctrl, oob, oob_len);
		else
			Nand_Config_Data_Spare(	&data_ctrl, data, data_len, g_pNF->EccType,\
									&spare_ctrl, oob, oob_len);		
		nand_readpage_ecc(chip_num, page_num, 0, g_pNF, &data_ctrl, &spare_ctrl);
	}     
	
	return FHA_SUCCESS;
}

T_U32 nand_write_callback(T_U32 chip_num, T_U32 page_num, const T_U8 *data, 
           T_U32 data_len, T_U8 *oob, T_U32 oob_len, T_U32 eDataType)
{ 
    T_NAND_ECC_STRU data_ctrl;
    T_NAND_ECC_STRU spare_ctrl;
    T_U32 boot_page_size = 0;
  	
	if(g_pNF == NULL)
	{
		printk(KERN_NOTICE "zz wrap_nand.c nand_write_callback() 404 g_pNF==NULL error!!!\n");
		return FHA_FAIL;
	}
   
    if(eDataType == FHA_DATA_BOOT)
    {
        boot_page_size = (data_len > 4096) ? 4096 : data_len;
        
        if (0 == page_num)
        {
            data_ctrl.buf = data;
            data_ctrl.buf_len = NAND_BOOT_DATA_SIZE;
            data_ctrl.ecc_section_len = NAND_BOOT_DATA_SIZE;
            data_ctrl.ecc_type = ECC_32BIT_P1KB;
        }
        else
        {                    
            data_ctrl.buf = data;
            data_ctrl.buf_len = boot_page_size;
            data_ctrl.ecc_section_len = 512;
            data_ctrl.ecc_type = g_pNF->EccType;
        }
        
    	nand_writepage_ecc(0, page_num, 0, g_pNF, &data_ctrl, AK_NULL);     	
    }
	else
	{		
    	//Nand_Config_Data(&data_ctrl, data, data_len, g_pNF->EccType);
		//Nand_Config_Spare(&spare_ctrl, oob, oob_len, g_pNF->EccType);
		if(g_pNF->PageSize  < NAND_PAGE_SIZE_2KB)
			Nand_Config_Data_Spare(	&data_ctrl, data, data_len, g_pNF->EccType,\
									&spare_ctrl, oob, oob_len);
		else
			Nand_Config_Data_Spare(	&data_ctrl, data, data_len, g_pNF->EccType,\
									&spare_ctrl, oob, oob_len);	
	
		nand_writepage_ecc(chip_num, page_num, 0, g_pNF, &data_ctrl, &spare_ctrl);
	}	
    
	return FHA_SUCCESS;
}

T_U32 nand_read_bytes_callback(T_U32 nChip, T_U32 rowAddr, T_U32 columnAddr, T_U8 *pData, T_U32 nDataLen)
{
	return nand_readbytes(nChip, rowAddr, columnAddr, g_pNF, pData, nDataLen);
}

void *ram_alloc(T_U32 size)
{
	return kmalloc(size, GFP_KERNEL);
}

void *ram_free(void *point)
{
	kfree(point);
	return NULL;
}

T_S32 print_callback(T_pCSTR s, ...)
{
	printk(KERN_INFO "%s", s);
	return AK_TRUE;
}


T_U32 init_globe_para(void)
{
	int ret = AK_FALSE;	

	if(g_pNand_Phy_Info != NULL && g_pNF != NULL)
	{
		return AK_TRUE;
	}
	g_pNand_Phy_Info = kmalloc(sizeof(T_NAND_PHY_INFO), GFP_KERNEL);
	if(g_pNand_Phy_Info == NULL)
	{
		printk(KERN_NOTICE "zz nand_char.c nand_char_ioctl() 471 g_pNand_Phy_Info=NULL error!!!\n");
		return AK_FALSE;
	}
	
	if(g_pinit_info!=NULL && g_pCallback!=NULL)
	{		
	
		ret = FHA_get_nand_para(g_pNand_Phy_Info);	
		
		if(ret == AK_FALSE)
		{
			printk(KERN_INFO "zz wrap_nand.c init_globe_para() 482 get nand para with fha lib error!!!\n");
			return AK_FALSE;
		}
	}
	else
	{
		printk(KERN_INFO "zz wrap_nand.c init_globe_para() 488 init fha lib fail!!!\n");
		return AK_FALSE;
	}
	

	g_pNF = kmalloc(sizeof(T_Nandflash_Add), GFP_KERNEL);
	if(g_pNF == NULL)
	{
		printk(KERN_NOTICE "zz nand_char.c nand_char_ioctl() 496 g_pNF==NULL error!!!\n");
		return AK_FALSE;
	}
	g_pNF->RowCycle      = g_pNand_Phy_Info->row_cycle;
	g_pNF->ColCycle      = g_pNand_Phy_Info->col_cycle;
	g_pNF->PageSize      = g_pNand_Phy_Info->page_size;
	g_pNF->PagesPerBlock = g_pNand_Phy_Info->page_per_blk;
	g_pNF->EccType       = (T_U8)((g_pNand_Phy_Info->flag & 0xf0) >> 4);  //4-7 bit is ecc type	
    
	switch(g_pNand_Phy_Info->page_size)
	{
		case 512:
		{		
			g_pNF->ChipType = NAND_512B_PAGE;
			break;
		}
		case 2048:
		{
			g_pNF->ChipType = NAND_2K_PAGE;
			break;
		}
		case 4096:
		{
			g_pNF->ChipType = NAND_4K_PAGE;
			break;
		}	
		case 8192:
		{
			g_pNF->ChipType = NAND_8K_PAGE;
			break;
		}
		default:
		{
			printk(KERN_INFO "zz wrap_nand.c init_globe_para() 529 g_pNand_Phy_Info->page_size=%d Error!!!\n",
				   g_pNand_Phy_Info->page_size);
		}
	}

	ak_nand_config_timeseq(g_pNand_Phy_Info->cmd_len, g_pNand_Phy_Info->data_len);
	
	printk(KERN_INFO "---------------------zz wrap_nand.c init_globe_para() 535------------------------\n");
	printk(KERN_INFO "----------------------------Nand Physical Parameter------------------------------\n");
	printk(KERN_INFO "chip_id = 0x%08lx\n", g_pNand_Phy_Info->chip_id);
	printk(KERN_INFO "page_size = %d\n", g_pNand_Phy_Info->page_size);
	printk(KERN_INFO "page_per_blk = %d\n", g_pNand_Phy_Info->page_per_blk);
	printk(KERN_INFO "blk_num = %d\n", g_pNand_Phy_Info->blk_num);
	printk(KERN_INFO "group_blk_num = %d\n", g_pNand_Phy_Info->group_blk_num);
	printk(KERN_INFO "plane_blk_num = %d\n", g_pNand_Phy_Info->plane_blk_num);
	printk(KERN_INFO "spare_size = %d\n", g_pNand_Phy_Info->spare_size);
	printk(KERN_INFO "col_cycle = %d\n", g_pNand_Phy_Info->col_cycle);
	printk(KERN_INFO "lst_col_mask = %d\n", g_pNand_Phy_Info->lst_col_mask);
	printk(KERN_INFO "row_cycle = %d\n", g_pNand_Phy_Info->row_cycle);
	printk(KERN_INFO "delay_cnt = %d\n", g_pNand_Phy_Info->delay_cnt);
	printk(KERN_INFO "custom_nd = %d\n", g_pNand_Phy_Info->custom_nd);
	printk(KERN_INFO "flag = 0x%08lx\n", g_pNand_Phy_Info->flag);
	printk(KERN_INFO "cmd_len = 0x%lx\n", g_pNand_Phy_Info->cmd_len);
	printk(KERN_INFO "data_len = 0x%lx\n", g_pNand_Phy_Info->data_len);
	printk(KERN_INFO "---------------------------------------------------------------------------------\n");
	
	return AK_TRUE;
}

void free_globe_para(void)
{
    if(g_pNand_Phy_Info != AK_NULL)
    {
        kfree(g_pNand_Phy_Info);
        g_pNand_Phy_Info = AK_NULL;
    }

    if(g_pNF != AK_NULL)
    {
        kfree(g_pNF);
        g_pNF = AK_NULL;
    }

    if(nand_data_buf != AK_NULL)
    {
        kfree(nand_data_buf);
        nand_data_buf = AK_NULL;
    }
}


#ifdef CONFIG_MTD_NAND_TEST   
void nand_balance_test_init(void)
{
    if (nand_erase_test)
        kfree(nand_erase_test);

    nand_total_blknum = g_pNand_Phy_Info->blk_num;
    nand_erase_test = (T_U32 *)kmalloc(nand_total_blknum*sizeof(T_U32), GFP_KERNEL);
    memset(nand_erase_test, 0, nand_total_blknum*sizeof(T_U32));
    //printk("nand_totalblk=%d##########\n",nand_total_blknum);
}

void nand_balance_test_exit(void)
{
    if (nand_erase_test)
    {
        kfree(nand_erase_test);
        nand_erase_test = NULL;
    }
}
#endif

T_U32 ak_mount_partitions(void)
{
	int i = 0;
	T_U8 *fs_info = NULL;
	int part_cnt = 0;
	struct partitions *parts = NULL;
	struct mtd_partition *pmtd_part = NULL;
	
	if(g_pinit_info!=NULL && g_pCallback!=NULL)
	{
		fs_info = kmalloc(g_pNF->PageSize, GFP_KERNEL);
		FHA_get_fs_part(fs_info, g_pNF->PageSize);
		part_cnt = *((int *)fs_info);  //calculate how many partitions
		#if ZZ_DEBUG
		printk(KERN_INFO "zz wrap_nand.c ak_mount_partitions() 573 part_cnt=%d\n", part_cnt);
		#endif
		if(part_cnt == 0)
		{
			printk(KERN_INFO "zz wrap_nand.c ak_mount_partitions() 577 part_cnt==0 error!!!\n");
			return AK_FALSE;
		}
		
		pmtd_part = kmalloc(sizeof(struct mtd_partition) * part_cnt, GFP_KERNEL);
		if(pmtd_part == NULL)
		{
			printk(KERN_INFO "zz wrap_nand.c ak_mount_partitions() 584 pmtd_part==NULL error!!!\n");
			return AK_FALSE;
		}		
		
		parts = (struct partitions *)(&fs_info[4]);
		
		for(i=0; i<part_cnt; i++)
		{	
			pmtd_part[i].name= kmalloc(MTD_PART_NAME_LEN, GFP_KERNEL);
			memcpy(pmtd_part[i].name, parts[i].name, MTD_PART_NAME_LEN);
			pmtd_part[i].size = parts[i].size;
			pmtd_part[i].offset = parts[i].offset;
			pmtd_part[i].mask_flags = parts[i].mask_flags;			
			printk(KERN_INFO "pmtd_part[%d]:\nname = %s\nsize = 0x%llx\noffset = 0x%llx\nmask_flags = 0x%x\n\n", 
					i, 
					pmtd_part[i].name, 
					pmtd_part[i].size,
					pmtd_part[i].offset,
					pmtd_part[i].mask_flags
				  );			
		}		
		
		add_mtd_partitions(g_master, (const struct mtd_partition *)pmtd_part, part_cnt);		
		kfree(pmtd_part);
		kfree(fs_info);
	}
	else
	{
		printk(KERN_INFO "ak_mount_partitions() init fha lib failed!!!\n");
	}

	return AK_TRUE;
	
}

int ak_nand_block_isbad(struct mtd_info *mtd, loff_t offs, int getchip)
{
	T_U32 blk_no = 0;
	T_U32 ret = 0;

	if (offs > mtd->size)
	{
		printk(KERN_INFO "zz wrap_nand.c ak_nand_block_isbad() 658 offs=0x%llx Error!!!\n", offs);
		return -EINVAL;
	}
	if(g_pinit_info==NULL || g_pCallback==NULL || g_pNF==NULL)
	{
		//printk(KERN_INFO "zz wrap_nand.c ak_nand_block_isbad() 663 point is NULL Error!!! \n");
		return 0;          //default good block
	}
	if(g_pNF->PageSize == 0 || g_pNF->PagesPerBlock == 0)
	{
		printk(KERN_INFO "zz wrap_nand.c ak_nand_block_isbad() 668 divisor=0  Error!!!\n");
		return 0;          //default good block
	}
	blk_no = div_u64(offs, (g_pNF->PageSize * g_pNF->PagesPerBlock));
	ret = FHA_check_bad_block(blk_no);     //divisor is 0 error!!!!!!
	#if ZZ_DEBUG
	printk(KERN_INFO "zz wrap_nand.c ak_nand_block_isbad() 674 blk_no=%d return %d\n", 
	       blk_no, ret);
	#endif
	
	return ret;
}

int ak_nand_block_markbad(struct mtd_info *mtd, loff_t offs)
{
	T_U32 blk_no = 0;
	T_U32 ret = 0;

	if (offs > mtd->size)
	{
		printk(KERN_INFO "zz wrap_nand.c ak_nand_block_markbad() 688 offs=0x%llx Error!!!\n", offs);
		return -EINVAL;
	}
	if(g_pinit_info==NULL || g_pCallback==NULL || g_pNF==NULL)
	{
		printk(KERN_INFO "zz wrap_nand.c ak_nand_block_markbad() 693 point is NULL Error!!!\n");
		return 0;  //default mark successed
	}
	if(g_pNF->PageSize == 0 || g_pNF->PagesPerBlock == 0)
	{
		printk(KERN_INFO "zz wrap_nand.c ak_nand_block_markbad() 698 divisor=0  Error!!!\n");
		return 0;          //default good block
	}	
	blk_no = div_u64(offs, (g_pNF->PageSize * g_pNF->PagesPerBlock));
	ret = FHA_set_bad_block(blk_no);
	#if ZZ_DEBUG
	printk(KERN_INFO "zz wrap_nand.c ak_nand_block_markbad() 704 blk_no=%d FHA_set_bad_block() return %d\n", 
	       blk_no, ret);
	#endif
	if(ret == FHA_SUCCESS)
	{
		return 0;  //mark successed
	}
	else
	{
		return 1;  //mark failed
	}
}

T_U32 try_nand_para(T_U32 chip_num, T_U32 page_num, T_U8 *data, 
           T_U32 data_len, T_U8 *oob, T_U32 oob_len, T_Nandflash_Add *p_add)
{
	T_NAND_ECC_STRU data_ctrl;    
	T_U8 pw[PASSWORD_LENGTH +1];
			
	data_ctrl.buf = data;
    data_ctrl.buf_len = data_len;
    data_ctrl.ecc_section_len = data_len + oob_len;
    data_ctrl.ecc_type = ECC_32BIT_P1KB;
   

	#if ZZ_DEBUG
	printk(KERN_INFO "zz wrap_nand.c try_nand_para() 736 chip_num=%d,page_jnum=%d,data_len=%d oob_len=%d\n",
	       chip_num, page_num, data_len, oob_len);
	#endif 
	
	nand_readpage_ecc(chip_num, page_num, 0, p_add, &data_ctrl, AK_NULL);

	memcpy(pw, data+PASSWORD_ADDR_OFFSET, PASSWORD_LENGTH);
	pw[PASSWORD_LENGTH] = '\0';
	#if ZZ_DEBUG
	printk(KERN_INFO "zz wrap_nand.c try_nand_para() 745 password string=%s", pw);
	#endif 
	return  AK_TRUE;
	if(!strcmp(pw, PASSWORD_STR))
	{	    
		#if ZZ_DEBUG
		int k = 0;
		printk(KERN_INFO "zz wrap_nand.c try_nand_para() 750 password match succussed!\n");
		for(k=50;k<500;k++)
		{
			if(k%10==0)
			{
				printk("\n");
			}
			printk(KERN_INFO "0x%02x\t", data[k]);
		}
		#endif 
		return  AK_TRUE;
	}
	return AK_FALSE;
}


//lgh
void ak_dump_datactrl_sparectrl(T_PNAND_ECC_STRU data_ctrl, T_PNAND_ECC_STRU spare_ctrl)	
{
		printk(KERN_INFO "data_ctrl: buf=%p, buf_len=%lu, ecc_section_len=%lu, ecc_type=%d\n",\
			data_ctrl->buf, data_ctrl->buf_len, data_ctrl->ecc_section_len, data_ctrl->ecc_type);
		printk(KERN_INFO "spare_ctrl: buf=%p, buf_len=%lu, ecc_section_len=%lu, ecc_type=%d\n",\
			spare_ctrl->buf, spare_ctrl->buf_len, spare_ctrl->ecc_section_len, spare_ctrl->ecc_type);
}


#if 0
unsigned char buf_r_data[8192];
unsigned char buf_w_data[8192];

void zz_test_nand(void)
{
    unsigned char oob_buf[32];
    int i = 0;
    int j = 0;
    int retval = 0;
    T_U32 chip = 0;
    int pagenum = 0;
	
	T_NAND_ECC_STRU data_ctrl;
    T_NAND_ECC_STRU spare_ctrl;
	nand_HWinit();
    for(i=10; i<15; i++)
    {
        retval = nand_eraseblock(chip, i*128, g_pNF);
        memset(buf_w_data, 0x9b, 8192);
        memset(oob_buf, i, 32);
        pagenum = i*128+10;
		
		Nand_Config_Data(&data_ctrl, buf_w_data, g_sNF.PageSize, g_sNF.EccType);
		Nand_Config_Spare(&spare_ctrl, oob_buf, YAFFS_OOB_SIZE, g_sNF.EccType); 
		
		retval = nand_writepage_ecc(chip, pagenum, 0, &g_sNF, &data_ctrl, &spare_ctrl);

        memset(buf_r_data,0xff, 8192);
        memset(oob_buf, 0xff, 32);

		Nand_Config_Data(&data_ctrl, buf_r_data, g_sNF.PageSize, g_sNF.EccType);
		Nand_Config_Spare(&spare_ctrl, oob_buf, YAFFS_OOB_SIZE, g_sNF.EccType); 

        retval = nand_readpage_ecc(chip, pagenum, 0, &g_sNF, &data_ctrl, &spare_ctrl);
        //retval = nand_readbytes(chip, pagenum, 0, &g_sNF, buf_r_data, 2048);
		
		for(j=0; j<50; j++)
		{
			if(j%10 == 0)
			{
				printk(KERN_INFO "\n");
			}
			printk(KERN_INFO "0x%x ", buf_r_data[j]);
		}
		printk(KERN_INFO "\n");
		#if 1
		for(j=0; j<10; j++)
		{
			printk(KERN_INFO "oob_buf[%d] = %d ", j,oob_buf[j]);
		}
		printk(KERN_INFO "\n");
		#endif
    }
}

void erase_all_flash(void)
{
	int i = 0;
	if((g_pNF == NULL) || (g_pNand_Phy_Info == NULL))
	{
		printk(KERN_INFO "zz wrap_nand.c erase_all_flash() 638 g_pNF == NULL || g_pNand_Phy_Info == NULL error!!!\n");
		return;
	}
	for(i=0; i<(g_pNand_Phy_Info->blk_num); i++)
	{
		nand_eraseblock(0, i*128, g_pNF);
	}
}

#endif


