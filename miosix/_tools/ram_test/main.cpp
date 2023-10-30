 /***************************************************************************
  *   Copyright (C) 2012 by Terraneo Federico                               *
  *                                                                         *
  *   This program is free software; you can redistribute it and/or modify  *
  *   it under the terms of the GNU General Public License as published by  *
  *   the Free Software Foundation; either version 2 of the License, or     *
  *   (at your option) any later version.                                   *
  *                                                                         *
  *   This program is distributed in the hope that it will be useful,       *
  *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
  *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
  *   GNU General Public License for more details.                          *
  *                                                                         *
  *   You should have received a copy of the GNU General Public License     *
  *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
  ***************************************************************************/

// RAM testing code
// Useful to test the external RAM of a board.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "sha1.h"
#include "miosix.h"

const unsigned int ramBase=0xC0000000; //Tune this to the right value
const unsigned int ramSize=512*1024*1024;     //Tune this to the right value

unsigned int rand_state = 0x7ad3c099;

bool shaCmp(unsigned int a[5], unsigned int b[5])
{
    if(memcmp(a,b,20)==0) return false;
    iprintf("Mismatch\n");
    for(int i=0;i<5;i++) iprintf("%04x",a[i]);
    iprintf("\n");
    for(int i=0;i<5;i++) iprintf("%04x",b[i]);
    iprintf("\n");
    return true;
}

template<typename T> bool ramTest()
{
    SHA1 sha;
    unsigned check_seed = rand_state;
    sha.Reset();
    for(unsigned int i=ramBase;i<ramBase+ramSize;i+=sizeof(T))
    {
        if ((i-ramBase) % (1024*1024) == 0) {
            iprintf("%08x\n", i);
            if (((i-ramBase) / (1024*1024)) & 1) miosix::led1On();
            else miosix::led1Off();
        }
        T *p=reinterpret_cast<T*>(i);
        T v=static_cast<T>(rand_r(&rand_state));
        *p=v;
        sha.Input(reinterpret_cast<const unsigned char*>(&v),sizeof(T));
    }
    miosix::led1Off();
    iprintf("finished write\n");
    unsigned int a[5];
    sha.Result(a);
    sleep(10); //To check SDRAM retention ability
    iprintf("readback\n");
    sha.Reset();
    int failures=0;
    int write_failures=0;
    T failure_mask=0;
    for(unsigned int i=ramBase;i<ramBase+ramSize;i+=sizeof(T))
    {
        volatile T *p=reinterpret_cast<T*>(i);
        T ck=static_cast<T>(rand_r(&check_seed));
        T v;
        int retries=0;
        do {
            v=*p;
            if (v!=ck) {
                miosix::led2On();
                failure_mask|=v^ck;
                if (retries==0) {
                    //iprintf("addr %08x read %08x != %08x expected (xor %08x)\n", i, v, ck, v^ck);
                    failures++;
                }
                retries++;
            } else {
                miosix::led2Off();
                //if (retries>0) iprintf("ok after %d retries\n", retries);
            }
        } while (v!=ck&&retries<2);
        if (v!=ck) {
            //iprintf("continued failure, bad write\n");
            write_failures++;
            miosix::led2Off();
        }
        sha.Input(reinterpret_cast<const unsigned char*>(&v),sizeof(T));
        if ((i+sizeof(T)-ramBase) % (1024*1024) == 0) {
            unsigned int start_addr=i+sizeof(T)-(1024*1024);
            iprintf("%08x-%08x failures=%10d failure_mask=%08x\n", start_addr, i+sizeof(T)-1, failures, failure_mask);
            failure_mask=0;
        }
    }
    iprintf("finished readback, failures=%d, write_failures=%d\n", failures, write_failures);
    unsigned int b[5];
    sha.Result(b);
    return shaCmp(a,b);
}

int main()
{
    for(;;)
    {
        iprintf("RAM test\nTesting word size transfers\n");
        ramTest<unsigned int>();
        iprintf("Testing halfword size transfers\n");
        ramTest<unsigned short>();
        iprintf("Testing byte size transfers\n");
        ramTest<unsigned char>();
        iprintf("Ok\n");
    }
}
