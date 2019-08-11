----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date:    05:12:16 01/23/2018 
-- Design Name: 
-- Module Name:    encrypter - Behavioral 
-- Project Name: 
-- Target Devices: 
-- Tool versions: 
-- Description: 
--
-- Dependencies: 
--
-- Revision: 
-- Revision 0.01 - File Created
-- Additional Comments: 
--
----------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.std_logic_unsigned.ALL;
-- Uncomment the following library declaration if using
-- arithmetic functions with Signed or Unsigned values
--use IEEE.NUMERIC_STD.ALL;

-- Uncomment the following library declaration if instantiating
-- any Xilinx primitives in this code.
--library UNISIM;
--use UNISIM.VComponents.all;

entity encrypter is
    Port ( clock : in  STD_LOGIC;
           K : in  STD_LOGIC_VECTOR (31 downto 0);
           P : in  STD_LOGIC_VECTOR (31 downto 0);
           C : out  STD_LOGIC_VECTOR (31 downto 0);
           reset : in  STD_LOGIC;
			  done: out STD_LOGIC;
           enable : in  STD_LOGIC);
end encrypter;

architecture Behavioral of encrypter is
	signal T : STD_LOGIC_VECTOR (3 downto 0) ;
	signal N : STD_LOGIC_VECTOR (5 downto 0);
	signal i: STD_LOGIC_VECTOR (5 downto 0);
   signal D: STD_LOGIC_VECTOR (31 downto 0);
	signal state: STD_LOGIC_VECTOR(1 downto 0);

begin
	process(clock, reset, enable)
	 begin
		if (reset = '1') then
			
			--resetting the values of internal signals
			
			i <= "000000";
			C <= "00000000000000000000000000000000";
			T <= "0000";
			N <= "000000";
			D <= "00000000000000000000000000000000"; 
			state <= "00";
			done <= '0';
			
		elsif (clock'event and clock = '1' ) then
			
			--actual iteration is happening here
			if(state = "00" and enable = '1') then
				D <= P;
				N <=	("00000"&K(0)) + ("00000"&K(1)) + ("00000"&K(2)) + ("00000"&K(3)) + ("00000"&K(4)) + 
						("00000"&K(5)) + ("00000"&K(6)) + ("00000"&K(7)) + ("00000"&K(8)) + ("00000"&K(9)) + 
						("00000"&K(10)) + ("00000"&K(11)) + ("00000"&K(12)) + ("00000"&K(13)) + ("00000"&K(14)) + 
						("00000"&K(15)) + ("00000"&K(16)) + ("00000"&K(17)) + ("00000"&K(18)) + ("00000"&K(19)) + 
						("00000"&K(20)) + ("00000"&K(21)) + ("00000"&K(22)) + ("00000"&K(23)) + ("00000"&K(24)) + 
						("00000"&K(25)) + ("00000"&K(26)) + ("00000"&K(27)) + ("00000"&K(28)) + ("00000"&K(29)) + 
						("00000"&K(30)) + ("00000"&K(31));
	
				T <=  (K(3)&K(2)&K(1)&K(0))		xor	(K(7)&K(6)&K(5)&K(4))		xor	(K(11)&K(10)&K(9)&K(8))		xor 
						(K(15)&K(14)&K(13)&K(12))	xor	(K(19)&K(18)&K(17)&K(16))	xor	(K(23)&K(22)&K(21)&K(20))	xor 
						(K(27)&K(26)&K(25)&K(24))	xor	(K(31)&K(30)&K(29)&K(28)); 

				state <= "01";
				i <="000000";	
				done <= '0';
			elsif (enable = '1')
			then
				if(i = N) then
					C <= D;
					done <= '1';
					state <="10";
				else
					D <= D xor T&T&T&T&T&T&T&T;
					T <= T + 1;
					i <= i + 1;
					done <= '0';
				end if;
			end if;
		end if;	
	 end process;

end Behavioral;

