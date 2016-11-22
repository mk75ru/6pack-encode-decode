#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>


#define SIXP_CHKSUM		0xFF	/* valid checksum of a 6pack frame */

typedef  struct  {
  int rx_count;
  unsigned char raw_buf[4];
  unsigned char cooked_buf[400];  
  unsigned int rx_count_cooked;  
} six_pack;

six_pack sp;


void  print_hex(const  char* data, size_t ldata, const char *fmt, ... )
{ 
       char* fmt_data;
       va_list ap;
       va_start(ap, fmt);
       vasprintf(&fmt_data,fmt,ap);  
       va_end(ap);
       printf("HEX: %s",fmt_data);
       int i;
       for(i=0;i < (int)ldata; i++){          
        printf("%.2hhx ",data[i]);
       }
       free(fmt_data);
       printf("\n");
}

int  calc_checksum(unsigned char *tx_buf,int length) { 
	unsigned char checksum = 0;
	int count = 0;
	for (count = 0; count < length; count++)
		checksum += tx_buf[count];
	return checksum;
}



/** 
 * @brief Упаковка пакета в sicpack
 * 
 * @param tx_buf_6pack //в этот буфер записывается упакованный пакет
 * @param buf          //это буфер  с данными для упаковки  
 * @param length_buf   //длина буфера с данными для упаковки 
 * 
 * @return длина упакованных данных
 */
static int encode_sixpack(unsigned char *tx_buf_6pack,unsigned char *buf,int length_buf) 
{
	unsigned char checksum = calc_checksum(buf,length_buf);
	buf[length_buf] = (unsigned char) 0xff - checksum;	
	printf("checksum    = %2.2x\n", buf[length_buf]);

	int count = 0;
	int count_6pack = 0;
	for (count = 0; count <= length_buf; count++) {
		if ((count % 3) == 0) {
			tx_buf_6pack[count_6pack++] = (buf[count] & 0x3f);
			tx_buf_6pack[count_6pack] = ((buf[count] >> 2) & 0x30);
		} else if ((count % 3) == 1) {
			tx_buf_6pack[count_6pack++] |= (buf[count] & 0x0f);
			tx_buf_6pack[count_6pack] =	((buf[count] >> 2) & 0x3c);
		} else {
			tx_buf_6pack[count_6pack++] |= (buf[count] & 0x03);
			tx_buf_6pack[count_6pack++] = (buf[count] >> 2);
		}

	}
	if ((length_buf % 3) != 2)
		count_6pack++;
        print_hex((const char*)tx_buf_6pack,count_6pack,"Print encode hex:(size=%d): \n",count_6pack);
	return count_6pack;
}


static void decode_data(int inbyte)
{
               	 
        unsigned char *buf;
	if (sp.rx_count != 3) {
		sp.raw_buf[sp.rx_count++] = inbyte;
		return;
	}
	
	buf = sp.raw_buf;
	sp.cooked_buf[sp.rx_count_cooked++] =
		buf[0] | ((buf[1] << 2) & 0xc0);
	sp.cooked_buf[sp.rx_count_cooked++] =
		(buf[1] & 0x0f) | ((buf[2] << 2) & 0xf0);
	sp.cooked_buf[sp.rx_count_cooked++] =
		(buf[2] & 0x03) | (inbyte << 2);
	sp.rx_count = 0;
}


static void decode_tail() {

        uint32_t rest = sp.rx_count;
	if (rest != 0) {
	  int i;
	  for (i = rest; i <= 3; i++) {
	  	 decode_data(0);
	  }
	}
	if (rest == 2)
	  sp.rx_count_cooked -= 2;
	else if (rest == 3)
	  sp.rx_count_cooked -= 1;
}

/** 
 * @brief Распаковка пакета 6pack, результат помещается в sp.cooked_buf,
 *        длина распакованных данных в sp.rx_count_cooked
 * @param rx_buf указатель на буфер запакованного  пакета
 * @param length длина запакованного пакета 
 */
static void decode_sixpack(unsigned char *rx_buf,int length)	
{
        //инициализация  структуры sixpack перед началом парсинга пакета
        sp.rx_count = 0;
	sp.rx_count_cooked = 0;
	//декодирование принятых данных результат помещается в sp.cooked_buf,
	//размер декодированных данных в sp.rx_count_cooked.
	int count1;
	for (count1 = 0; count1 < length; count1++) {
          decode_data(rx_buf[count1]);
	}
	//декодирование оставшегося хвоста,результат дописывается sp.cooked_buf,
	//счетчик  sp.rx_count_cooked увеличивается на длину хвоста.
	decode_tail();
        print_hex((const char*)(sp.cooked_buf),sp.rx_count_cooked,"Print decode hex:(size=%d) :\n",sp.rx_count_cooked);
	//вычисляется и сверяется контрольная сумма
	unsigned char checksum =  calc_checksum(sp.cooked_buf,sp.rx_count_cooked);
	if (checksum != SIXP_CHKSUM) {
	   printf( "6pack: bad_checksum %2.2x\n", checksum);
	} else {
	   printf("6pack: ok checksum %2.2x\n", checksum);
	}
	sp.rx_count_cooked--;//уменьшение счетчика для отбрасывани байта контрольной суммы.
}


unsigned  char bufs_udp[] =  {
  0x01, 0x96, 0x9e, 0x9c, 0xa8, 0xaa, 0xa4, 0x62,
  0x96, 0x9e, 0x9c, 0xa8, 0xaa, 0xa4, 0x67,
  0x03, 0xcc, 0x45, 0x00, 0x00, 0x22, 0x00, 0x00, 0x40, 0x00, 0x40, 0x11, 0x36, 0xc6,
  0x01, 0x01, 0x01, 0x03,
  0x01, 0x01, 0x01, 0x01,
  0x92, 0x25, 0x11, 0x5c, 0x00, 0x0e, 0xc4, 0xb7,
  0x31, 0x31, 0x31, 0x31, 0x31, 0x31
};


unsigned  char bufs[] =  {
  0x01 ,0x06 ,0x26 ,0x27 ,0x1c ,0x28 ,0x2a ,0x2a ,0x24 ,0x22 ,0x1a ,0x25
 ,0x1e ,0x2c ,0x24 ,0x2a ,0x2a ,0x24 ,0x2b ,0x19 ,0x03 ,0x0c ,0x31 ,0x11 ,0x00 ,0x00 ,0x02 ,0x08
 ,0x00 ,0x00 ,0x00 ,0x10 ,0x00 ,0x00 ,0x11 ,0x04 ,0x36 ,0x06 ,0x31 ,0x00 ,0x01 ,0x01 ,0x03 ,0x00 
 ,0x01 ,0x01 ,0x01 ,0x00 ,0x01 ,0x02 ,0x25 ,0x09 ,0x11 ,0x0c ,0x14 ,0x00 ,0x0e ,0x04 ,0x33 ,0x2d 
 ,0x31 ,0x01 ,0x0d ,0x0c ,0x31 ,0x01 ,0x0d ,0x0c ,0x09 ,0x00
};





int  main(){
  unsigned char tx_buf_6pack[400];
  size_t len_tx_buf_6pack = encode_sixpack(tx_buf_6pack,bufs_udp,sizeof(bufs_udp));
  decode_sixpack(tx_buf_6pack,len_tx_buf_6pack);
  printf("\n------------------------------------------------------------------------------\n");
  decode_sixpack(bufs,sizeof(bufs));  
  unsigned char tx_buf_6pack2[400];
  size_t len_tx_buf_6pack2 = encode_sixpack(tx_buf_6pack2,sp.cooked_buf,sp.rx_count_cooked);  
  printf("\n------------------------------------------------------------------------------\n");
  decode_sixpack(tx_buf_6pack2,len_tx_buf_6pack2);

}



