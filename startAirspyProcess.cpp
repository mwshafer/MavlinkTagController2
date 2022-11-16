#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h> 
#include <sys/wait.h>
#include <iostream>
#include <errno.h>
#include <string.h>

void startAirspyHF(bool* signal)
{
   	std::system("./airspyhf_rx -f 146 -m on -a 192000 -g on -l high -t 0 -r /home/pi/data/airspy_hf.dat -n 5760000");
   	*signal = true;
}

void startAirspyMini(bool* signal)
{
   	std::system("airspy_rx -r /home/pi/data/airspy_mini.dat -f 146 -a 3000000 -h 21 -t 0 -n 90000000");
   	*signal = true;
}
