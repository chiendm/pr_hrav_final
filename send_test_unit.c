/*
 ============================================================================
 Name        : send_unit_test.c
 Author      :
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */
#ifndef NULL
#define NULL   ((void *) 0)
#endif
typedef enum { FALSE, TRUE } bool;
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/stat.h>
#define PR_FILE_0				"pr_core0.bit"
#define PR_FILE_1				"pr_core1.bit"
#define DMA_PKT_LEN 			1472
#define BUFFER_LEN_DEFAULT 		60
#define BUFFER_HALF_LEN_DEFAULT	(BUFFER_LEN_DEFAULT/2)
#define	DMA_PKT_MAGIC_NUMBER_0	0xfe
#define DMA_PKT_MAGIC_NUMBER_1	0xca
#define DMA_PKT_MAGIC_NUMBER_2	0xae
#define DMA_BUF_SIZE 			(DMA_PKT_LEN + 1)
#define MIN_DMA_PKT_LEN 		60
#define DMA_BUFF_INFO 			24
#define CORE_0 					0x0
#define CORE_1 					0x1
int main(void) {
	char buffer[BUFFER_LEN_DEFAULT];
	int pos = 0,
		file_size = 0,
		buffer_size = BUFFER_LEN_DEFAULT;
	FILE *fin = fopen("testfile_tiny.txt","r");
	if(!fin){
		printf("Can't open the file %s\n", "testfile_tiny.txt");
	}
	fseek(fin, 0, SEEK_END);
	file_size = ftell(fin);
	printf("Size of read file: %ld\n",file_size);
	rewind(fin);
	printf("Size of  sending buffer: %ld\n",(sizeof(buffer)/sizeof(char)));
	while(pos!=file_size){
		if(file_size - pos < BUFFER_LEN_DEFAULT){
			buffer_size = file_size - pos;
			pos += fread(buffer, 1, buffer_size, fin);
		}
		else
			pos += fread(buffer, 1, buffer_size, fin);
		/*printf("Positon: %d\n",pos);
		printf("Buffer Size: %d\n",buffer_size);*/

		/*if(file_exist(PR_FILE_0)){
		 	static FILE* fp = fopen(PR_FILE_0,"r");
			cli_bbf_static_scanbuff(buffer, buffer_size, NULL,
				NULL, NULL, 0, NULL, NULL, NULL,fp, CORE_0);
		}
		else if(file_exist(PR_FILE_1)){
		 	static FILE* fp = fopen(PR_FILE_1,"r");
			cli_bbf_static_scanbuff(buffer, buffer_size, NULL,
				NULL, NULL, 0, NULL, NULL, NULL,fp, CORE_1);
		}
		else*/
			cli_bbf_static_scanbuff(buffer, buffer_size, NULL, NULL, NULL, 0, NULL, NULL, NULL);
	}

	return 0;
}
int cli_bbf_static_scanbuff(const unsigned char *buffer, uint32_t length, const char **virname,
		const struct cli_bm_patt **patt, const struct cli_matcher *root,
		uint32_t offset, const struct cli_target_info *info, struct cli_bm_off *offdata,
		uint32_t *viroffset)
{
//	struct timespec start, end, packet_time_start, packet_time_end;
//	printf("bufferID is %d\n",buffer_ID);
    struct packet_header
    {
        char magic[3];
        char type;
        char bufferID[3];
        char status;
    };
    typedef struct packet_header HEADER; // 3 + 1 + 3 + 1 = 8 (bytes)

    struct send_packet
    {
        HEADER header; // 8
        char info[DMA_BUFF_INFO]; // 24
        int length; //length of data: 4
        char data[DMA_BUF_SIZE]; // 1471 : de danh 1 byte NULL cho STRING
    };
    typedef struct send_packet PACKET; // 8 + 24 + 4 + 1472 = 1508

    struct send_buffer
    {
        int length; // 4
        char buffer[DMA_BUF_SIZE]; // 1473
    };
    typedef struct send_buffer BUFFER; // 4 + 1476 = 1480
    PACKET sendPacket;
    BUFFER sendBuffer;

    /*Buffer pointer*/
    int pos_0 = 0;//begin of left-half-part buffer (ie. core_0)
    int pos_1 = length/2;//begin of right-half-part buffer (ie. core_1)

    char first = TRUE;
    char last = FALSE;//last packet of core 0
    char last_ = FALSE;//last packet of core 1
    memset(sendPacket.info, '\xFF', DMA_BUFF_INFO);

    // Prepare packet
    int cli_send_DMA_result = 0;
    int packet_count = 0;
    while(!(last&&last_))
    {
        packet_count++ ;
        cli_send_DMA_result = 0;
        /* Clear all sending buffer */
        sendPacket.length = 0;
        bzero(sendPacket.data,DMA_BUF_SIZE);
        sendBuffer.length = 0;
        bzero(sendBuffer.buffer,DMA_BUF_SIZE);
        int dataSize;
        if(first) // first packet for core 0
        {
            dataSize = DMA_PKT_LEN - sizeof(HEADER) - sizeof(sendPacket.info);
        }
        else
        {
            dataSize = DMA_PKT_LEN - sizeof(HEADER);
        }
        //last packet for core_0
        if(packet_count%2==1){
			last = (dataSize<(length/2 - pos_0))?FALSE:TRUE;
			dataSize = last?(length/2-pos_0):dataSize;
        }
        //last packet for core_1
        else{
        	last_ = (dataSize<(length - pos_1))?FALSE:TRUE;
        	dataSize = last_?(length - pos_1):dataSize;
        }


        //calculate packet length
        int packetSize;
        packetSize = dataSize + sizeof(HEADER) +( first?sizeof(sendPacket.info):0);
        /* Configure packet Header */
        sendPacket.header.magic[0] = DMA_PKT_MAGIC_NUMBER_0;
        sendPacket.header.magic[1] = DMA_PKT_MAGIC_NUMBER_1;
        sendPacket.header.magic[2] = DMA_PKT_MAGIC_NUMBER_2;

        sendPacket.header.type = (packet_count%2==1)?CORE_0:CORE_1; //packet type, odd packet for CORE_0, even packet for CORE_1
        sendPacket.header.bufferID[0] = 0xFF;
        sendPacket.header.bufferID[1] = 0xFF;
        sendPacket.header.bufferID[2] = 0xFF;
        // Configure packet status
        if(packetSize >= MIN_DMA_PKT_LEN){
            sendPacket.header.status &= 0x00; // set the 6bit MSB = 0.
        }
        else
        {
            sendPacket.header.status = 0xFC & (packetSize << 2); // set 6bit MSB = packetsize;
        }

        sendPacket.header.status = (first)?(sendPacket.header.status|0x02):(sendPacket.header.status & 0xFD); //if first set status[1] to 1.
        if(packet_count%2==1)
        	sendPacket.header.status = (last)?(sendPacket.header.status|0x01):(sendPacket.header.status & 0xFE); //if last set status[0] to 1.
        else
        	sendPacket.header.status = (last_)?(sendPacket.header.status|0x01):(sendPacket.header.status & 0xFE); //if last set status[0] to 1.
        /* Configure Buffer Info */
        if(first)
        {
            //setting extra data for current data bufer at the first packet
        }

        /* Configure packet data */
        if(packet_count%2==1)
        	memcpy(sendPacket.data,buffer + pos_0,dataSize);
        else
        	memcpy(sendPacket.data,buffer + pos_1,dataSize);
        sendPacket.length = dataSize;

        /* Copy packet to sending buffer */
        //copy header
        memcpy(sendBuffer.buffer,&sendPacket.header,sizeof(HEADER));
        //copy buffer info and data to send
        if(first)
        {
            //copy buffer info
            memcpy(sendBuffer.buffer+sizeof(HEADER),sendPacket.info,DMA_BUFF_INFO);
            //copy buffer data.
            memcpy(sendBuffer.buffer+sizeof(HEADER)+DMA_BUFF_INFO,sendPacket.data,sendPacket.length);
        }
        else
        {
            memcpy(sendBuffer.buffer+sizeof(HEADER),sendPacket.data,sendPacket.length);
        }
        sendBuffer.length = packetSize;
        print_hex(sendBuffer.buffer,sendBuffer.length,packet_count,sendPacket.header.type);
        /* Begin transfer*/
        //        PrintInHex("\nsendbuff = ",sendBuffer.buffer,sendBuffer.length);
//    	printf("Time start:\n");
//    	clock_gettime(CLOCK_MONOTONIC, &packet_time_start);

        /*int written_bytes = write(current_send_sockfd,sendBuffer.buffer,sendBuffer.length);
        if (written_bytes <= 0)
        {
            printf("\nWrite data error!\n");
        }*/
//        current_send_sockfd++;
//        if(current_send_sockfd == 7) current_send_sockfd = send_nf1_sockfd;

//		clock_gettime(CLOCK_MONOTONIC, &packet_time_end);
//		printf("Time end:\n");
//		print_timespec(end);
//		struct timespec timeElapsed = timespecDiff(packet_time_start , packet_time_end);
//		double packet_time = get_time_us(timeElapsed); //(double)timeElapsed;
//		printf("  SENDING PACKET TIME : %.3f ms\n", packet_time);

        /*increase buffer pointer*/
        if(packet_count%2 == 1) // core 0
        	pos_0 += dataSize;
        else					// core 1
        	pos_1 += dataSize;

        if(packet_count==2)// first packet for core 1
        	first=FALSE;
    } // end while(!last)
    /*if(sendPacket.header.type == 0x1)
    	printf("packet_count on core 0 = %d\n",packet_count);
    else if (sendPacket.header.type == 0x5)
    	printf("packet_count on core 1 = %d\n",packet_count);
    return 0;*/
    printf("Finished...\n");
}

void print_hex(unsigned char* buf, int count, int packet, char core)
{
	int i;
	printf("Core %d - packet #%d: \n",core,packet);
	for(i = 0; i < count; i++)
	{
		printf("%x ", buf[i]);
		if((i&0x1F) == 31) printf("\n");
	}
	printf("\n");
}

int file_exist (char *filename)
{
  struct stat   buffer;
  return (stat (filename, &buffer) == 0);
}
