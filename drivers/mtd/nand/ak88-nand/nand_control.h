/**
 * @filename nand_control.h
 * @brief AK880x nandflash driver
 *
 * This file describe how to control the AK880x nandflash driver.
 * Copyright (C) 2008 Anyka (GuangZhou) Software Technology Co., Ltd.
 * @author yiruoxiang
 * @modify jiangdihui
 * @date 2007-1-10
 * @version 1.0
 * @ref
 */
#ifndef __NAND_CONTORL_H__
#define __NAND_CONTORL_H__

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup NandFlash Architecture NandFlash Interface
 *  @ingroup Architecture
 */
/*@{*/

/* contorl/status register */
#define NF_REG_CTRL_STA_CMD_DONE  (1UL << 31)
#define NF_REG_CTRL_STA_CE0_SEL   (1 << 10)
#define NF_REG_CTRL_STA_CE1_SEL   (1 << 11)
#define NF_REG_CTRL_STA_STA_CLR   (1 << 14)

/* dma cpu control register */
#define BIT_DMA_CMD_DONE          0x80000000

/* ECC control bit */
#define ECC_CTL_DIR_READ        (0<<2)
#define ECC_CTL_ADDR_CLR        (1<<4)
#define ECC_CTL_NFC_EN          (1<<20)
#define ECC_CTL_BYTE_CFG(m)     ((m)<<7)  
#define ECC_CTL_MODE(n)         ((n)<<22)
#define ECC_CTL_START           (1<<3)
#define ECC_CTL_DEC_EN          (1<<1)
#define ECC_CTL_RESULT_NO_OK    (1<<27)
#define ECC_CTL_END             (1<<6)
#define ECC_CTL_NO_ERR          (1<<26)
#define ECC_CTL_DIR_WRITE       (1<<2)
#define ECC_CTL_ENC_EN          (1<<0)
#define ECC_CTL_DEC_RDY         (1<<24)
#define ECC_CTL_ENC_RDY         (1<<23)

#define ECC_CHECK_NO_ERROR                              (0x1<<26)
#define ECC_ERROR_REPAIR_CAN_NOT_TRUST                  (0x1<<27)

#define DATA_ECC_CHECK_OK                               1
#define DATA_ECC_CHECK_ERROR                            0
#define DATA_ECC_ERROR_REPAIR_CAN_TRUST                 2
#define DATA_ECC_ERROR_REPAIR_CAN_NOT_TRUST             3

//  |               page               |
//  | data | fs(file system) | parity  |
//  | data |          spare            |
#define NAND_DATA_SIZE            512  
#define NAND_FS_SIZE              4
#define NAND_PARITY_SIZE_MODE1    13
#define NAND_PARITY_SIZE_MODE0    7

/// @cond NANDFLASH_DRV
//******************************************************************************************//
//ͨ��CTRL reg0��������ѹջ��˵�����¡�����7��confiuration��1��flag��
//*************reg0~reg19�������������***********
/*
1,nandflash�������������ϴ������ݵ�����
a,COMMAND_CYCLES_CONF:��ʾ����������д�����Nandflash���
b,ADDRESS_CYCLES_CONF:��ʾ����������д�����Nandflash��ַ��Ϣ
c,WRITE_DATA_CONF:��ʾ����������д���FIFO�е�������Ϣ��
���ڶ������ݿ��ܷ��ڲ�ͬλ�ã��������������ֶ���������
d��READ_DATA_CONF����ʾ���������߶����ݣ��������ݷ���FIFO��
e��READ_CMD_CONF����������̫�ã��Ұ�����ΪREAD_INFO_CONF
  ��ʾ���������϶�����״̬����ID��Ϣָ��,(Ҳ�����ڶ�ȡС��
  8 byte�����������������ݷ���reg20

  ���⣬�������ECC�Ķ���оƬ���Զ���data��Ϣ���ļ�ϵͳ��Ϣ�ŵ���
  Ӧ��FIFO�ͼĴ�����

2,��nandflash�޹أ�Ϊ��������ʱ��ȷ����дԤ����ȴ����ʱ���������
f,DELAY_CNT_CONF ����nandflash����������Ӧ�����Լ��3us��50us����ʱ��
  ���ݲŻ�׼���ã���������������ʱʱ����bit11~bit22��д��
g.WAIT_JUMP_CONF ��Nandflash�����У����˿��Զ�status �����ѯ״̬�⣬
����һ��R/B����������������ȷ����д�����Ƿ���ɡ�������ȴ������Ӵ�����

��������ÿ����ѡ��ֻ��ѡһ��������ͨ��reg0��ַѹ������ջ�У�����ʵ�ֶ�
nandflash�����в���
��������һ��ָ��������LAST_CMD_FLAG��־
*/

//////// command sequece configuration define ////////////

/* command cycle's configure:
 *      CMD_END=X, ALE=0, CLE=1, CNT_EN=0,  (BIT[0:3])
 *      REN=0, WEN=1, CMD_EN=1, STAFF_EN=0, (BIT[4:7])
 *      DAT_EN=0, RBN_EN=0, CMD_WAIT=0,     (BIT[8:10])
 */
#define COMMAND_CYCLES_CONF     0x64
/* address cycle's:
 *      CMD_END=X, ALE=1, CLE=0, CNT_EN=0,  (BIT[0:3])
 *      REN=0, WEN=1, CMD_EN=1, STAFF_EN=0, (BIT[4:7])
 *      DAT_EN=0, RBN_EN=0, CMD_WAIT=0,     (BIT[8:10])
 */
#define ADDRESS_CYCLES_CONF     0x62

/*  read data cycle's:
 *      CMD_END=X, ALE=0, CLE=0, CNT_EN=1,  (BIT[0:3])
 *      REN=1, WEN=0, CMD_EN=0, STAFF_EN=0, (BIT[4:7])
 *      DAT_EN=1, RBN_EN=0, CMD_WAIT=0,     (BIT[8:10])
 */
#define READ_DATA_CONF          0x118

/*  write data cycle's:
 *      CMD_END=X, ALE=0, CLE=0, CNT_EN=1,  (BIT[0:3])
 *      REN=0, WEN=1, CMD_EN=0, STAFF_EN=0, (BIT[4:7])
 *      DAT_EN=1, RBN_EN=0, CMD_WAIT=0,     (BIT[8:10])
 */
#define WRITE_DATA_CONF         0x128

/* read command status's:(example: read ID(pass), status(pass), �ض����ݱȽ��ٵĲ���?
//����оƬ���ڶ�ȡstatus ID��Ϣ��оƬ�Ѹ���Ϣ�ᵽ��Ӧ�Ĵ���(8 byte)��������FIFO�������ѯNandflash״̬��
 *      CMD_END=X, ALE=0, CLE=0, CNT_EN=1,  (BIT[0:3])
 *      REN=1, WEN=0, CMD_EN=1, STAFF_EN=0, (BIT[4:7])
 *      DAT_EN=0, RBN_EN=0, CMD_WAIT=0,     (BIT[8:10])
 */
#define READ_INFO_CONF          0x58

//wait delay time enable bit
#define DELAY_CNT_CONF          (1<<10)

//wait R/b enable bit
#define WAIT_JUMP_CONF          (1<<9)

// last command's bit0 set to 1
#define LAST_CMD_FLAG           (1<<0)


//#######excute comamnd,ctrl reg22 command configuration define ################
//���е����ö�����Է�������ķ�ʽ��
//�������Ƭѡ��Ƭ����������ģʽ���Լ��Ƿ�ʹ�ܵ�ʡ��ģʽflag
//Ƭѡʹ�ú�NCHIP_SELECT(x)
#define NCHIP_SELECT(x)         ((0x01 << (x)) << 10)

/*
bit0(save mode) = 0;
bit1-bit8(staff_cont)=0
bit9(watit save mode jump) = 0
bit10-bit13(chip select)=0
bit14(sta_clr)=0
bit15-bit18(write protect)=0
bit19-bit29 reserved =0;
bit30(start control) = 1;
bit31(check statu) = 0;
*/
#define DEFAULT_GO                  ((1 << 30)|(1<<9))


/** @{@name Command List and Status define
 *  Define command set and status of nandflash
 */
#define NFLASH_READ1                0x00
#define NFLASH_READ2                0x30
#define NFLASH_READ1_HALF           0x01
#define NFLASH_READ22               0x50

#define NFLASH_COPY_BACK_READ       0x35
#define NFLASH_COPY_BACK_WRITE      0x85
#define NFLASH_COPY_BACK_WRITE1     0x8A
#define NFLASH_COPY_BACK_CONFIRM    0x10
#define NFLASH_RESET                0xff

#define NFLASH_FRAME_PROGRAM0       0x80
#define NFLASH_FRAME_PROGRAM1       0x10

#define NFLASH_BLOCK_ERASE0         0x60
#define NFLASH_BLOCK_ERASE1         0xD0
#define NFLASH_STATUS_READ          0x70
#define NFLASH_READ_ID              0x90

//status register bit
#define NFLASH_PROGRAM_SUCCESS      0x01 //bit 0
#define NFLASH_HANDLE_READY         0x40 //bit 6
#define NFLASH_WRITE_PROTECT        0x80 //bit 7

/*@}*/
#ifdef __cplusplus
}
#endif

#endif //__NAND_CONTORL_H__
