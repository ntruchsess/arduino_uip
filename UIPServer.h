/*
 UIPServer.h - Arduino implementation of a uIP wrapper class.
 Copyright (c) 2013 Norbert Truchsess <norbert.truchsess@t-online.de>
 All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef UIPSERVER_H
#define UIPSERVER_H

#import "Server.h"
#import "UIPClient.h"

class UIPServer : Server {

public:
  UIPServer(uint16_t);
  UIPClient available();
  void begin();
  size_t write(uint8_t);
  size_t write(const uint8_t *buf, size_t size);
  using Print::write;

private:
  uint16_t _port;
};

#endif
