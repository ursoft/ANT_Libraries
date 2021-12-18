/*
This software is subject to the license described in the License.txt file
included with this software distribution. You may not use this file except
in compliance with this license.

Copyright (c) Dynastream Innovations Inc. 2016
All rights reserved.
*/
#ifndef TEST_H
#define TEST_H

#include "types.h"

#define CHANNEL_TYPE_MASTER   (0)
#define CHANNEL_TYPE_SLAVE    (1)
#define CHANNEL_TYPE_INVALID  (2)

void Test_Start();
void Test_Init(UCHAR ucDeviceNumber_, UCHAR ucChannelType_);

#endif //TEST_H
