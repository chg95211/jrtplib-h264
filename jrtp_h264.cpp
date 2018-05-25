#include <rtpsession.h>
#include <rtpudpv4transmitter.h>
#include <rtpipv4address.h>
#include <rtpsessionparams.h>
#include <rtperrors.h>
#include <rtplibraryversion.h>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

using namespace jrtplib;

//  RTP:
//  -----------------------------------------------------------------
//  0               8              16              24              32
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  | V |P|X|  CC   |M|     PT      |               SN              |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                           Timestamp                           |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                             SSRC                              |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                             CSRC                              |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                             ...                               |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

//  NALU:
//  -----------------
//  0               8
//  +-+-+-+-+-+-+-+-+
//  |F|NRI|   Type  |
//  +-+-+-+-+-+-+-+-+

#define MAXLEN	(RTP_DEFAULTPACKETSIZE - 100)

void checkerror(int rtperr)
{
    if (rtperr < 0)
    {
        std::cout << "ERROR: " << RTPGetErrorString(rtperr) << std::endl;
        exit(-1);
    }
}

static int findStartCode1(unsigned char *buf)
{
    if(buf[0]!=0 || buf[1]!=0 || buf[2]!=1)                // 判断是否为0x000001,如果是返回1
        return 0; 
    else 
        return 1;
}

static int findStartCode2(unsigned char *buf)
{
    if(buf[0]!=0 || buf[1]!=0 || buf[2]!=0 || buf[3]!=1)   // 判断是否为0x00000001,如果是返回1
        return 0;
    else 
        return 1;
}

void naluPrintf(unsigned char *buf, unsigned int len, unsigned char type, unsigned int count)
{
    unsigned int i=0;

    printf("NALU type=%d num=%d len=%d : \n", type, count, len);

    #if 0	
    for(i=0; i<len; i++)
    {
        printf(" %02X", buf[i]);
        if(i%32 == 31)
            printf("\n");
    }
    printf("\n");
    #endif
}

void rtpPrintf(unsigned char *buf, unsigned int len)
{
    unsigned int i=0;

    printf("RTP len=%d : \n", len);

    for(i=0; i<len; i++)
    {
        printf(" %02X", buf[i]);
        if(i%32 == 31)
            printf("\n");
    }

    printf("\n");
}

void naluToRtp(RTPSession* sess, unsigned char *buf, unsigned int len)
{
    int status;
    unsigned char rtpbuf[2000]={0};
    unsigned int num=0;
    unsigned int more=0;
    unsigned int pi=0;

    unsigned char nalu_header = buf[0];
    unsigned int rtplen = len-1;

    unsigned char fu_indicator = 0;
    unsigned char fu_header = 0;

    if(rtplen <= MAXLEN)
    {	
        //rtpPrintf(buf, len);

        status = sess->SendPacket(buf, len, 96, true, 3600); 
        checkerror(status); 		
    }
	else
	{
        num = rtplen / MAXLEN;
        more = rtplen % MAXLEN;

        if(more == 0)
        {
            num -= 1;
            more = MAXLEN;
        }

        fu_indicator = nalu_header & 0x60;   // bit 7 6 5
        fu_indicator |= 0x1C;                // bit 4 3 2 1 0    28    FU-A

        fu_header = nalu_header & 0x1F;      // bit 4 3 2 1 0
			
        while(pi <= num)
        {
            if( pi==0 )          // 第一包
            {	
                fu_header |= 0x80;    // bit 7  S
                fu_header &= ~0x40;   // bit 6  E 
                fu_header &= ~0x20;   // bit 5  R

                rtpbuf[0] = fu_indicator;
                rtpbuf[1] = fu_header;
                memcpy(&rtpbuf[2], &buf[1], MAXLEN);				
                //rtpPrintf(rtpbuf, MAXLEN+2);

                status = sess->SendPacket(rtpbuf, MAXLEN+2, 96, true, 0);  
                checkerror(status); 
            }
			else if(pi == num)   // 最后一包
			{
                fu_header &= ~0x80;    // bit 7  S
                fu_header |= 0x40;     // bit 6  E 
                fu_header &= ~0x20;    // bit 5  R

                rtpbuf[0] = fu_indicator;
                rtpbuf[1] = fu_header;
                memcpy(&rtpbuf[2], &buf[1+pi*MAXLEN], more);			
                //rtpPrintf(rtpbuf, more+2);

                status = sess->SendPacket(rtpbuf, more+2, 96, true, 3600);  
                checkerror(status); 	
            }
            else                // 中间包
            {
                fu_header &= ~0x80;    // bit 7  S
                fu_header &= ~0x40;    // bit 6  E 
                fu_header &= ~0x20;    // bit 5  R

                rtpbuf[0] = fu_indicator;
                rtpbuf[1] = fu_header;
                memcpy(&rtpbuf[2], &buf[1+pi*MAXLEN], MAXLEN);				
                //rtpPrintf(rtpbuf, MAXLEN+2);

                status = sess->SendPacket(rtpbuf, MAXLEN+2, 96, true, 0);  
                checkerror(status);
            }	
            pi++;	
        }		
    }
}

int main(void)
{	
    //int num;
    int status;

    RTPSession sess;
    uint16_t portbase = 6666;  
    uint16_t destport = 6664;
    uint8_t destip[]={192, 168, 2, 105};

    RTPUDPv4TransmissionParams transparams;
    RTPSessionParams sessparams;

    /* set h264 param */
    sessparams.SetUsePredefinedSSRC(true);         // 设置使用预先定义的SSRC    
    sessparams.SetOwnTimestampUnit(1.0/90000.0);   // 设置采样间隔 
    sessparams.SetAcceptOwnPackets(false);         // 接收自己发送的数据包  

    transparams.SetPortbase(portbase);
    status = sess.Create(sessparams,&transparams);	
    checkerror(status);

    RTPIPv4Address addr(destip,destport);
    status = sess.AddDestination(addr);
    checkerror(status);

    sess.SetDefaultTimestampIncrement(3600);       // 设置时间戳增加间隔
    sess.SetDefaultPayloadType(96);
    sess.SetDefaultMark(true);   

    RTPTime delay(0.040);
    RTPTime starttime = RTPTime::CurrentTime();

    FILE *fd;
    unsigned char file_end=0;
    unsigned int nalu_count = 0;
    unsigned char buf[1024*2024]={0};
    unsigned int i=0;
    unsigned char nalu_type=0;

    fd = fopen("./test.h264", "rb");
    if (!fd)
    {
        printf("error: can not open file !\n");

        return -1;
    }
	
    /* .h264文件最开始数据为 00 00 01 或 00 00 00 01*/
    fread(buf, 1, 4, fd);
    if(findStartCode1(buf))                     // 00 00 01
    {	
        i = 3;
        nalu_type = 3;
    }	
    else if(findStartCode2(buf))                // 00 00 00 01
    {
        i = 4;
        nalu_type = 4;
    }
    else
    {
        printf("error: .h264 file wrong!\n");
    }
	
    while( !file_end )    
    {		
        if( feof(fd) )    
        {
            //printf(" NALU   num=%d   len=%d \r\n", nalu_count, i-nalu_type-1); 	
            naluToRtp(&sess, buf+nalu_type, i-nalu_type-1);

            file_end = 1;

            printf("end\n");
        }

        buf[i++] = fgetc(fd);

        if( findStartCode2(&buf[i-4]) )         // 00 00 00 01
        {
            //printf(" NALU   num=%d   len=%d \r\n", nalu_count, i-nalu_type-4);
            naluToRtp(&sess, buf+nalu_type, i-nalu_type-4);	

            nalu_count++;
            buf[0] = 0x00;
            buf[1] = 0x00;
            buf[2] = 0x00;
            buf[3] = 0x01;
            i = 4;	
            nalu_type = 4;		

            RTPTime::Wait(0.03);			
        }
        else if( findStartCode1(&buf[i-3]) )    // 00 00 01
        {		
            //printf(" NALU   num=%d   len=%d \r\n", nalu_count, i-nalu_type-3);
            naluToRtp(&sess, buf+nalu_type, i-nalu_type-3);

            nalu_count++;
            buf[0] = 0x00;
            buf[1] = 0x00;
            buf[2] = 0x01;
            i = 3;
            nalu_type = 3;

            RTPTime::Wait(0.03);
        }
    } 

    fclose(fd);

    return 0;	
}

