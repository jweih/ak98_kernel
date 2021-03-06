/**
 * @filename nand_char.c
 * @brief AK880x nandflash char device driver
 * Copyright (C) 2010 Anyka (Guangzhou) Software Technology Co., LTD
 * @author zhangzheng
 * @modify 
 * @date 2010-10-20
 * @version 1.0
 * @ref Please refer to��
 */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/string.h>
#include <asm/uaccess.h>
#include <linux/mtd/partitions.h>
#include <mtd/mtd-abi.h>
#include <mach-anyka/nand_list.h>
#include <mach-anyka/fha.h>
#include "arch_nand.h"
#include "nand_control.h"
#include "wrap_nand.h"

#define NAND_CHAR_MAJOR                 168
#define AK_NAND_PHY_ERASE               0xa0
#define AK_NAND_PHY_READ                0xa1
#define AK_NAND_PHY_WRITE               0xa2
#define AK_NAND_GET_CHIP_ID             0xa3
#define AK_MOUNT_MTD_PART               0xa4
#define AK_NAND_READ_BYTES              0xa5
#define AK_GET_NAND_PARA                0xa6

#define ZZ_DEBUG                        0

extern T_NAND_PHY_INFO *g_pNand_Phy_Info;
extern T_PNandflash_Add g_pNF;
extern T_PFHA_INIT_INFO g_pinit_info;
extern T_PFHA_LIB_CALLBACK g_pCallback;

static int nand_char_open(struct inode *inode, struct file *filp);
static int nand_char_close(struct inode *inode, struct file *filp);
static int nand_char_ioctl(struct inode *inode, struct file *filp,
		     unsigned int cmd, unsigned long arg);

struct erase_para
{
    uint32_t chip_num;
    uint32_t startpage;
};

struct rw_para
{
    T_U32 chip_num;
    T_U32 page_num;
    T_U8 *data;
    T_U32 data_len;
    T_U8 *oob;
    T_U32 oob_len;
	T_U32 eDataType;
};

struct rbytes_para
{
	T_U32 chip_num; 
	T_U32 row_addr; 
	T_U32 clm_addr; 
	T_U8 *data;
	T_U32 data_len;
};

struct get_id_para
{
    uint32_t chip_num;
    uint32_t *nand_id;
};

static struct nand_char_dev
{
    struct cdev c_dev;
}nand_c_dev;

extern struct mtd_info *g_master;

static int nand_c_major = NAND_CHAR_MAJOR;

static const struct file_operations nand_char_fops =
{
    .owner   = THIS_MODULE,
    .open    = nand_char_open,
    .release = nand_char_close,
    .ioctl   = nand_char_ioctl
};

static int nand_char_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &nand_c_dev;
	#if ZZ_DEBUG
    printk(KERN_NOTICE "zz nand_char.c nand_char_open() 104\n");
	#endif
    return 0;
}

static int nand_char_close(struct inode *inode, struct file *filp)
{
	#if ZZ_DEBUG
    printk(KERN_NOTICE "zz nand_char.c nand_char_close() 112\n");
	#endif
    return 0;
}

static int nand_char_ioctl(struct inode *inode, struct file *filp,
		                     unsigned int cmd, unsigned long arg)
{
	#if ZZ_DEBUG
    printk(KERN_NOTICE "zz nand_char.c nand_char_ioctl() 121 cmd=0x%x\n", cmd);   
	#endif
    switch(cmd)
    {
        case AK_NAND_PHY_ERASE:
        {
            struct erase_para ep;
			if(g_pNF == NULL)
			{
				#if ZZ_DEBUG
				printk(KERN_NOTICE "zz nand_char.c nand_char_ioctl() 131 g_pNF==NULL\n");
				#endif
				break;
			}
            copy_from_user(&ep, (struct erase_para *)arg, sizeof(struct erase_para));
			#if ZZ_DEBUG
            printk(KERN_INFO "zz nand_char.c nand_char_ioctl() 137 AK_NAND_PHY_ERASE\n");
            printk(KERN_INFO "erase chip_num = %d startpage = %d\n",
                   ep.chip_num,
                   ep.startpage);
			#endif
            nand_eraseblock(ep.chip_num, ep.startpage, g_pNF);
            break;
        }
        case AK_NAND_PHY_READ:
        {
            struct rw_para r_para;
            T_NAND_ECC_STRU data_ctrl;
            T_NAND_ECC_STRU spare_ctrl;
            T_U8 oob_buf[32];

			if(g_pNF == NULL)
			{
				#if ZZ_DEBUG
				printk(KERN_NOTICE "zz nand_char.c nand_char_ioctl() 155 g_pNF==NULL\n");
				#endif
				break;
			}
            copy_from_user(&r_para, (struct rw_para*)arg, sizeof(struct rw_para));
			#if ZZ_DEBUG
            printk(KERN_INFO "zz nand_char.c nand_char_ioctl() 161 AK_NAND_PHY_READ\n");
            printk(KERN_INFO "Read chip_num=%d page_num=%d data_len=%d oob_len=%d eDataType=%d\n",
                   r_para.chip_num,
                   r_para.page_num,
                   r_para.data_len,
                   r_para.oob_len,
                   r_para.eDataType);
			#endif
	
            if(r_para.eDataType == FHA_DATA_BOOT)
    		{
    			data_ctrl.buf = kmalloc(r_para.data_len, GFP_KERNEL);
    			data_ctrl.buf_len = r_para.data_len;
    			data_ctrl.ecc_section_len = NAND_DATA_SIZE + r_para.oob_len;
    			data_ctrl.ecc_type = ECC_8BIT_P512B;

    			spare_ctrl.buf = oob_buf;
    			spare_ctrl.buf_len = r_para.oob_len;
    			spare_ctrl.ecc_section_len = NAND_DATA_SIZE + r_para.oob_len;
    			spare_ctrl.ecc_type = ECC_8BIT_P512B;
    		}
			else if (r_para.eDataType == FHA_DATA_ASA || r_para.eDataType == FHA_DATA_BIN)
			{
				data_ctrl.buf = kmalloc(r_para.data_len, GFP_KERNEL);
    			data_ctrl.buf_len = r_para.data_len;
    			data_ctrl.ecc_section_len = NAND_DATA_SIZE + r_para.oob_len;
    			data_ctrl.ecc_type = g_pNF->EccType;

    			spare_ctrl.buf = oob_buf;
    			spare_ctrl.buf_len = r_para.oob_len;
    			spare_ctrl.ecc_section_len = NAND_DATA_SIZE + r_para.oob_len;
    			spare_ctrl.ecc_type = g_pNF->EccType;
			}
			else
			{
    			Nand_Config_Data(&data_ctrl,
					             kmalloc(r_para.data_len, GFP_KERNEL),
					             r_para.data_len, 
					             g_pNF->EccType
					             );
				Nand_Config_Spare(&spare_ctrl, oob_buf, r_para.oob_len, g_pNF->EccType);
			}      
            nand_readpage_ecc(r_para.chip_num, r_para.page_num, 0, g_pNF, &data_ctrl, &spare_ctrl);

            copy_to_user(((struct rw_para *)arg)->data, data_ctrl.buf, r_para.data_len);
            copy_to_user(((struct rw_para *)arg)->oob, spare_ctrl.buf, r_para.oob_len);

            kfree(data_ctrl.buf);
            break;
        }
        case AK_NAND_PHY_WRITE:
        {
            struct rw_para w_para;
            T_NAND_ECC_STRU data_ctrl;
            T_NAND_ECC_STRU spare_ctrl;
            unsigned char oob_buf[32];
			if(g_pNF == NULL)
			{
				#if ZZ_DEBUG
				printk(KERN_NOTICE "zz nand_char.c nand_char_ioctl() 220 g_pNF==NULL\n");
				#endif
				break;
			}
            copy_from_user(&w_para, (struct rw_para *)arg, sizeof(struct rw_para));
			#if ZZ_DEBUG
            printk(KERN_INFO "zz nand_char.c nand_char_ioctl() 226 AK_NAND_PHY_WRITE\n");
            printk(KERN_INFO "Write chip_num=%d page_num=%d data_len=%d oob_len=%d eDataType=%d\n",
                   w_para.chip_num,
                   w_para.page_num,
                   w_para.data_len,
                   w_para.oob_len,
                   w_para.eDataType);
    		#endif

			if(w_para.eDataType == FHA_DATA_BOOT)
    		{
    			data_ctrl.buf = kmalloc(w_para.data_len, GFP_KERNEL);
    			data_ctrl.buf_len = w_para.data_len;
    			data_ctrl.ecc_section_len = NAND_DATA_SIZE + w_para.oob_len;
    			data_ctrl.ecc_type = ECC_8BIT_P512B;

    			spare_ctrl.buf = oob_buf;
    			spare_ctrl.buf_len = w_para.oob_len;
    			spare_ctrl.ecc_section_len = NAND_DATA_SIZE + w_para.oob_len;
    			spare_ctrl.ecc_type = ECC_8BIT_P512B;
    		}
			else if (w_para.eDataType == FHA_DATA_ASA || w_para.eDataType == FHA_DATA_BIN)
			{
				data_ctrl.buf = kmalloc(w_para.data_len, GFP_KERNEL);
    			data_ctrl.buf_len = w_para.data_len;
    			data_ctrl.ecc_section_len = NAND_DATA_SIZE + w_para.oob_len;
    			data_ctrl.ecc_type = g_pNF->EccType;

    			spare_ctrl.buf = oob_buf;
    			spare_ctrl.buf_len = w_para.oob_len;
    			spare_ctrl.ecc_section_len = NAND_DATA_SIZE + w_para.oob_len;
    			spare_ctrl.ecc_type = g_pNF->EccType;
			}
			else
			{
				Nand_Config_Data(&data_ctrl, 
					             kmalloc(w_para.data_len, GFP_KERNEL), 
					             w_para.data_len, 
					             g_pNF->EccType
					             );
				Nand_Config_Spare(&spare_ctrl, oob_buf, w_para.oob_len, g_pNF->EccType);
			}	
			
			if(data_ctrl.buf != NULL)
			{
				copy_from_user(data_ctrl.buf, ((struct rw_para *)arg)->data, data_ctrl.buf_len);
				copy_from_user(spare_ctrl.buf, ((struct rw_para *)arg)->oob, spare_ctrl.buf_len);
			}
			else
			{
				printk(KERN_INFO "zz nand_char.c nand_char_ioctl() 276 data_ctrl.buf == NULL error!!!\n");
				break;
			}
			
            nand_writepage_ecc(w_para.chip_num, w_para.page_num, 0, g_pNF, &data_ctrl, &spare_ctrl);

            kfree(data_ctrl.buf);
            break;
        }
        case AK_NAND_GET_CHIP_ID:
        {
            uint32_t nand_id = 0;
            struct get_id_para gip;
            copy_from_user(&gip, (struct get_id_para *)arg, sizeof(struct get_id_para));
            nand_id = nand_read_chipID(gip.chip_num);
			#if ZZ_DEBUG
            printk(KERN_INFO "zz nand_char.c nand_char_ioctl() 292 AK_NAND_GET_CHIP_ID\n");
            printk(KERN_INFO "mtd_ioctl() chip_num=%d nand_id=0x%x\n", 
                   gip.chip_num,
                   nand_id);
			#endif
            copy_to_user(((struct get_id_para *)arg)->nand_id, &nand_id, 
                           sizeof(nand_id));
            break;
        }
        case AK_MOUNT_MTD_PART:
		{
			int retval = 0;
			T_U32 part_cnt = 0;
			int i = 0;
			T_U8 *fs_info = NULL;
			struct partitions *parts = NULL;
			struct mtd_partition *pmtd_part = NULL;
			
			retval = init_fha_lib();        //initialize fha lib
			
			if(retval == FHA_SUCCESS)
			{
				if(g_pNF == NULL)
				{
					printk(KERN_INFO "zz nand_char.c nand_char_ioctl() 316 g_pNF == NULL error!!!\n");
					break;
				}
				fs_info = kmalloc(g_pNF->PageSize, GFP_KERNEL);
				retval = FHA_get_fs_part(fs_info, g_pNF->PageSize);
				
				#if ZZ_DEBUG
				printk(KERN_INFO "zz nand_char.c nand_char_ioctl() 323 FHA_get_fs_part() return %d\n", retval);
				printk(KERN_INFO "----------------zz nand_char.c nand_char_ioctl() 324 test recieve data start------------------\n");
				for(i=0; i<80; i++)
				{	
					if(i%10 == 0)
					{
						printk(KERN_INFO "\n");
					}
					printk(KERN_INFO "0x%02x\n", fs_info[i]);
				}
				printk(KERN_INFO "----------------zz nand_char.c nand_char_ioctl() 333 test recieve data end---------------------\n");
				#endif
				
				part_cnt = *((int *)fs_info);  //calculate how many partitions
				
				if(part_cnt == 0)
				{
					printk(KERN_INFO "zz nand_char.c nand_char_ioctl() 340 part_cnt==0 error!!!\n");
					break;
				}
				
				pmtd_part = kmalloc(sizeof(struct mtd_partition) * part_cnt, GFP_KERNEL);
				
				if(pmtd_part == NULL)
				{
					printk(KERN_INFO "zz nand_char.c nand_char_ioctl() 348 pmtd_part==NULL error!!!\n");
					break;
				}
				#if ZZ_DEBUG
				printk(KERN_INFO "---------------zz nand_char.c nand_char_ioctl() 352 AK_MOUNT_MTD_PART mtd parts info start-------------\n");
				#endif
				
				parts = (struct partitions *)(&fs_info[4]);
				
				for(i=0; i<part_cnt; i++)
				{	
					pmtd_part[i].name = kmalloc(MTD_PART_NAME_LEN, GFP_KERNEL);
					memcpy(pmtd_part[i].name, (parts+i)->name, MTD_PART_NAME_LEN);
					pmtd_part[i].size = parts[i].size;
					pmtd_part[i].offset =  parts[i].offset;
					pmtd_part[i].mask_flags = parts[i].mask_flags;
					
					#if ZZ_DEBUG
					printk(KERN_INFO "pmtd_part[%d]:\nname = %s\nsize = 0x%llx\noffset = 0x%llx\nmask_flags = 0x%x\n\n", 
					       i, 
					       pmtd_part[i].name, 
					       pmtd_part[i].size,
					       pmtd_part[i].offset,
					       pmtd_part[i].mask_flags);
					#endif
				}
				
				#if ZZ_DEBUG
				printk(KERN_INFO "----------------zz nand_char.c nand_char_ioctl() 376 AK_MOUNT_MTD_PART mtd part info end-----------------\n");
				#endif
				
				add_mtd_partitions(g_master, (const struct mtd_partition *)pmtd_part, part_cnt);
				
				kfree(pmtd_part);
				kfree(fs_info);
			}
			else
			{
				printk(KERN_INFO "zz nand_char.c nand_char_ioctl() 386 AK_MOUNT_MTD_PART init fha lib failed!!!\n");
			}
			
		    break;
        }
		case AK_NAND_READ_BYTES:            
		{
			struct rbytes_para rbp;
			if(g_pNF == NULL)
			{
				printk(KERN_NOTICE "zz nand_char.c nand_char_ioctl() 396 g_pNF==NULL Error!!!\n");
				break;
			}
			copy_from_user(&rbp, (struct rbytes_para *)arg, sizeof(struct rbytes_para));
			rbp.data = kmalloc(rbp.data_len, GFP_KERNEL);
			nand_readbytes(rbp.chip_num, rbp.row_addr, rbp.clm_addr, g_pNF, rbp.data, rbp.data_len);
			copy_to_user(((struct rbytes_para *)arg)->data, rbp.data, rbp.data_len);
			break;
		}
		case AK_GET_NAND_PARA:
		{
			g_pNand_Phy_Info = kmalloc(sizeof(T_NAND_PHY_INFO), GFP_KERNEL);
			if(g_pNand_Phy_Info == NULL)
			{
				printk(KERN_NOTICE "zz nand_char.c nand_char_ioctl() 410 g_pNand_Phy_Info==NULL Error!!!\n");
				break;
			}
			copy_from_user(g_pNand_Phy_Info, (T_NAND_PHY_INFO *)arg, sizeof(T_NAND_PHY_INFO));
			g_pNF = kmalloc(sizeof(T_Nandflash_Add), GFP_KERNEL);
			if(g_pNF == NULL)
			{
				printk(KERN_NOTICE "zz nand_char.c nand_char_ioctl() 417 g_pNF==NULL Error!!!\n");
				break;
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
					printk(KERN_INFO "zz nand_char.c nand_char_ioctl() 451 globle g_pNF->ChipType is error!!!\n");
				}
			}
			
			#if 1  //ZZ_DEBUG
			printk(KERN_INFO "----------------------zz nand_char.c nand_char_ioctl() 456-----------------------\n");
			printk(KERN_INFO "----------------------------Nand Physical Parameter------------------------------\n");
			printk(KERN_INFO "chip_id = 0x%x\n", g_pNand_Phy_Info->chip_id);
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
			printk(KERN_INFO "flag = 0x%x\n", g_pNand_Phy_Info->flag);
			printk(KERN_INFO "cmd_len = 0x%x\n", g_pNand_Phy_Info->cmd_len);
			printk(KERN_INFO "data_len = 0x%x\n", g_pNand_Phy_Info->data_len);
			printk(KERN_INFO "---------------------------------------------------------------------------------\n");
			#endif
			break;
		}
	    default:
        {   
            return -EINVAL;
        }
    }
    return 0;
}

static struct class *nand_class;

static void nand_setup_cdev(void)
{
    int err = 0;
    dev_t devno = MKDEV(nand_c_major, 0);

    cdev_init(&(nand_c_dev.c_dev), &nand_char_fops);
    nand_c_dev.c_dev.owner = THIS_MODULE;
    nand_c_dev.c_dev.ops = &nand_char_fops;
    err = cdev_add(&(nand_c_dev.c_dev), devno, 1);
    if(err)
    {
        printk(KERN_NOTICE "Error %d adding anyka nand char dev\n", err);
    }
	
	//automatic mknod device node
	nand_class = class_create(THIS_MODULE, "nand_class");
	device_create(nand_class, NULL, devno, &nand_c_dev, "nand_char");
}

static int __init nand_char_init(void)
{
    int result = 0;
    dev_t devno = MKDEV(nand_c_major, 0);
    if(nand_c_major)
    {
        result = register_chrdev_region(devno, 1, "anyka nand char dev");
    }
    else
    {
        result = alloc_chrdev_region(&devno, 0, 1, "anyka nand char dev");
    }
    if(result<0)
    {
        return result;
    }
    nand_setup_cdev();
	printk(KERN_INFO "Nand Char Device Initialize Successed!\n");
    return 0;
}
                      
static void __exit nand_char_exit(void)
{
	dev_t devno = MKDEV(nand_c_major, 0);
	
	//destroy device node
	device_destroy(nand_class, devno);
	class_destroy(nand_class); 
	
	//delete char device
    cdev_del(&(nand_c_dev.c_dev));
    unregister_chrdev_region(devno, 1);
}
                      
module_init(nand_char_init);
module_exit(nand_char_exit);            
MODULE_LICENSE("GPL");
MODULE_AUTHOR("ZhangZheng");
MODULE_DESCRIPTION("Direct character-device access to Nand devices");

