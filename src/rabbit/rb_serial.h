/* rb_serial.h
 */
 
#ifndef RB_SERIAL_H
#define RB_SERIAL_H

// 0=incomplete, <0=malformed, 1..4=length
int rb_vlq_decode(int *dst,const void *src,int srcc);

#endif
