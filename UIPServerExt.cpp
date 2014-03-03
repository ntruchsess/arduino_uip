/*
 UIPServer.cpp - Arduino implementation of a uIP wrapper class.
 Copyright (c) 2013 Norbert Truchsess <norbert.truchsess@t-online.de>
 All rights reserved.

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
  */
#include "UIPEthernet.h"
#include "UIPServer.h"
extern "C" {
  #include "utility/uip-conf.h"
}
#include "HardwareSerial.h"

UIPServerExt::UIPServerExt(uint16_t p) :
    UIPServer(p)
{
}

UIPClientExt UIPServerExt::available()
{
  UIPEthernetClass::tick();
  for ( uip_userdata_t* data = &UIPClient::all_data[0]; data < &UIPClient::all_data[UIP_CONNS]; data++ )
    {
      if (data->packets_in[0] != NOBLOCK)
        {
          if ((data->state & UIP_CLIENT_CONNECTED) && uip_conns[data->state & UIP_CLIENT_SOCKETS].lport ==_port)
            {
              Serial.println("connected and data");
              return UIPClientExt(&uip_conns[data->state & UIP_CLIENT_SOCKETS]);
            }
          if ((data->state & UIP_CLIENT_REMOTECLOSED) && ((uip_userdata_closed_t *)data)->lport == _port)
            {
              Serial.println("connected and no data");
              return UIPClientExt(data);
            }
        }
    }
  return UIPClientExt();
}
