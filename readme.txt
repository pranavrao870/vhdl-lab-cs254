Team Name   :   Logic Geeks

Members   :   160050020, 160100021, 160100024, 16D070017

We have implemented both the mandatory and the optional part

VHDL Compilation : 
	Simply put all given files in a single folder and make the
	top_level.vhdl file as top module and generate the bit file.
	Then use this bit file to generate .xsvf file, which is then programmed on the boards.

C Compilation :
	There are two files here. main.c and relay.py
	The former is the program that is to be run on backend computer and the latter is used on computer which acts as relay
	(for optional part).

UART Communication :

	OPTIONAL PART:

	We have implemented the optional part by using relay computer.
	The main fpga board under observation is connected to one computer and the other board is connected to previous board via the uart port through a relay computer. Both boards are programmed with the same .xsvf file generated. 
	The relay computer also serves as the backend computer for the other board. 

	NOTE : We have connected the other board to a backend computer via fgpa link because otherwise we would need to wait a lot of time for the board to time out in S2 state before moving forward to S3 or S4. Although the extra board does not need the fpga link to a computer to send UART data.

	How to Run : 
		The relay computer runs the relay.py on a terminal which checks that if uart data arrives from one board it transfers it to other.
		Then run backend program on both computer and then the Uart data is sent simply using slider switches in state S4 of one board (as given in problem statement) which is read in state S5 of other.

	MANDATORY PART:

	The UART communication here is done via gtkterm. Keep the gtkterm console open and use it to communicate 
with the uart port of the fpga board.

Actual running of the project:
	Copy the main.c to the folder upper-directory/20140524/makestuff/apps/flcli
	Run the command: make here or if you are in upper-directory/20140524/makestuff/hdlmake/apps/makestuff/swled/cksum/vhdl,
	run the script ./build_host.sh.
	
	
	OPTIONAL PART:
		Generate .xsvf using ISE.
		Use the .xsvf file generated to program the boards.
		Start relay.py in the relay computer.
		Reset the boards.
		To run the computers (central controller and relay) use the command 
		sudo ./flcli -v 1d50:602b:0002 -z in the folder containing the executable and track_data.csv

		
	MANDATORY:
		Generate .xsvf using ISE.
		Use the .xsvf file generated to program the board.
		Run the script ./build_fpga.sh from the directory upper-directory/20140524/makestuff/hdlmake/apps/makestuff/swled/cksum/vhdl to establish connection, program, and run the program.
		or use the command sudo ./flcli -v 1d50:602b:0002 -z in the folder containing the executable and track_data.csv
		
		Reset the board 
		To run the computers (central controller and relay) use the command 
		sudo ./flcli -v 1d50:602b:0002 -z in the folder containing the executable and track_data.csv
		
		Start gtkterm for communicating with the boards UART port with the configuration:
			Baud rate : 115200 
			stopbits : 1 (default) 
			parity : none (default)	
		Use the command: sudo gtkterm -p /dev/ttyXRUSB0 -s 115200 to start gtkterm if the uart port of fpga is connected at /dev/ttyXRUSB0
