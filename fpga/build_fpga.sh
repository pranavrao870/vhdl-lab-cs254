#!/bin/bash

# Establish connection with the board
sudo ../../../../../../apps/flcli/lin.x64/rel/flcli -v 1d50:602b:0002 -i 1443:0007

# Program the board with fpga3.xsvf
sudo ../../../../../../apps/flcli/lin.x64/rel/flcli -v 1d50:602b:0002 -p J:D0D2D3D4:fpga3.xsvf

# Start the host side

sudo ../../../../../../apps/flcli/lin.x64/rel/flcli -v 1d50:602b:0002 -z