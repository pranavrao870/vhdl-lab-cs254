 

--
-- Copyright (C) 2009-2012 Chris McClelland
--
-- This program is free software: you can redistribute it and/or modify
-- it under the terms of the GNU Lesser General Public License as published by
-- the Free Software Foundation, either version 3 of the License, or
-- (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU Lesser General Public License for more details.
--
-- You should have received a copy of the GNU Lesser General Public License
-- along with this program.  If not, see <http://www.gnu.org/licenses/>.
--
library ieee;

use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.std_logic_unsigned.all;

architecture rtl of swled is

    component baudrate_gen is
    port (clk    : in std_logic;
            rst    : in std_logic;
            sample: out std_logic);
    end component baudrate_gen;
    
    component uart_rx is            
    port (clk    : in std_logic;
            rst    : in std_logic;
            rx        : in std_logic;
            sample: in STD_LOGIC;
            rxdone: out std_logic;
            rxdata: out std_logic_vector(7 downto 0));
    end component uart_rx;
--    signal rxdone : std_logic;
--    signal rxdata : std_logic_vector(7 downto 0);
    
    component uart_tx is            
    port (clk    : in std_logic;
            rst    : in std_logic;
            txstart: in std_logic;
            sample : in std_logic;
            txdata : in std_logic_vector(7 downto 0);
            txdone : out std_logic;
            tx        : out std_logic);
    end component uart_tx;


    component debouncer is
    Generic (wait_cycles : STD_LOGIC_VECTOR (19 downto 0) := x"F423F");
    Port ( clk : in  STD_LOGIC;
           button : in  STD_LOGIC;
           button_deb : out  STD_LOGIC);
    end component;

    component encrypter
        port( clock : in  STD_LOGIC;
           K : in  STD_LOGIC_VECTOR (31 downto 0);
           P : in  STD_LOGIC_VECTOR (31 downto 0);
           C : out  STD_LOGIC_VECTOR (31 downto 0);
           reset : in  STD_LOGIC;
           done : out STD_LOGIC;
           enable : in  STD_LOGIC);
    end component;

    component decrypter
        port( clock : in  STD_LOGIC;
           K : in  STD_LOGIC_VECTOR (31 downto 0);
           C : in  STD_LOGIC_VECTOR (31 downto 0);
           P : out  STD_LOGIC_VECTOR (31 downto 0);
           reset : in  STD_LOGIC;
           done : out STD_LOGIC;
           enable : in  STD_LOGIC);
    end component;

    -- Flags for display on the 7-seg decimal points
    signal flags                   : std_logic_vector(3 downto 0);

    -- Registers implementing the channels
    signal reg0_next         : std_logic_vector(7 downto 0);
    signal led_val        :    std_logic_vector(7 downto 0);

    signal temp_ans : std_logic_vector(63 downto 0);
    signal ans : std_logic_vector(63 downto 0);
    signal recvdData : std_logic_vector(31 downto 0);

    -- will encrypt all these at first and store it in flip flops
    signal grid_loc, encr_grid_loc, ack1, encr_ack1, ack2, encr_ack2 :    std_logic_vector(31 downto 0);
    signal change : std_logic;
    signal pos2: unsigned(2 downto 0);
    signal counter        :    std_logic_vector(35 downto 0);

    signal reset_encr, reset_decr: std_logic;
    signal key: std_logic_vector(31 downto 0);
    signal done_encrypt, done_decrypt, deb_res, lb, rb, ub, db : std_logic;
    signal encr_in, encr_out, decr_in, decr_out, encr_sw_in : std_logic_vector(31 downto 0);
    signal curr_state: std_logic_vector(4 downto 0);
    signal macro_state: std_logic_vector(2 downto 0);
    signal send_bits: std_logic_vector(7 downto 0);
    signal uart_tx_counter : std_logic_vector(3 downto 0);
    signal s6_counter : std_logic_vector(31 downto 0);
    signal uart_tx_data, txdata : std_logic_vector(7 downto 0);
    signal uart_rx_data, rxdata : std_logic_vector(7 downto 0);
    signal uart_ans : std_logic_vector(63 downto 0);
    signal tx_start, rxdone, txdone, uart_change : std_logic;
    signal sample: std_logic;
    signal connection_counter:  std_logic_vector(31 downto 0);
    
begin                                                                     --BEGIN_SNIPPET(registers)
    -- Infer registers
    i_brg : baudrate_gen port map (clk => clk_in, rst => deb_res, sample => sample);
    
    i_rx : uart_rx port map( clk => clk_in, rst => deb_res,
                            rx => rx, sample => sample,
                            rxdone => rxdone, rxdata => rxdata);

    i_tx : uart_tx port map( clk => clk_in, rst => deb_res,
                            txstart => tx_start,
                            sample => sample, txdata => txdata,
                            txdone => txdone, tx => tx);
    deb_reset: debouncer
        port map(clk => clk_in,
                    button => reset_in,
                    button_deb => deb_res);

    left_press: debouncer
        port map(clk => clk_in,
                    button => lb1,
                    button_deb => lb);

    right_press: debouncer
        port map(clk => clk_in,
                    button => rb1,
                    button_deb => rb);

    up_press: debouncer
        port map(clk => clk_in,
                    button => ub1,
                    button_deb => ub);

    down_press: debouncer
        port map(clk => clk_in,
                    button => db1,
                    button_deb => db);

    
    encrypt: encrypter
              port map (clock => clk_in,
                        reset => reset_encr,
                        P => encr_in,
                        enable => '1',
                        C => encr_out,
                        done => done_encrypt,
                        K => key);

    decrypt: decrypter
              port map (clock => clk_in,
                        reset => reset_decr,
                        C => decr_in,
                        enable => '1',
                        P => decr_out,
                        done => done_decrypt,
                        K => key);

process(clk_in, deb_res)
    begin
            if ( deb_res = '1' ) then
                -- values of encr_grid_loc, encr_ack1, eccr_ack2 doesn't matter
                --reducing the number of LUTS, so encrypting them one after the other
                ack1 <= "10011100111110011101000100000101";
                ack2 <= "01001011110101010010111101010101";
                     temp_ans <= (others => '0');
                     ans <= (others => '0');
                     encr_in <= (others => '0');
                key <= "10101001010010010100100101001001";
                led_val <= "11111111";
                temp_ans <= (others => '0');
                ans <= (others => '0');
                recvdData <= (others => '0');

                pos2 <= "000";
                grid_loc <= "00000000000000000000000000100010";
                counter <= x"000000000";
                curr_state <= "00000";
                macro_state <= "000";
                uart_tx_counter <= "0000";
                uart_ans <= (others => '0');
                uart_tx_data <= (others => '0');
                s6_counter <= (others => '0');
                connection_counter <= x"00000000";
            elsif ( rising_edge(clk_in) ) then
                if (macro_state = "000" and counter = x"008954400") 
                then
                    macro_state <= "001";
                    counter <= x"000000000";
                    curr_state <= "00000";
                    led_val <= "00000000";
                elsif (macro_state = "000")
                then
                    counter <= counter + 1;

                elsif (macro_state = "001") 
                then
                    if (curr_state = "00000")
                    then
                        reset_encr <= '1';
                        encr_in <= grid_loc;
                        curr_state <= "00001";
                    --    led_val <= "00000000";

                    elsif (curr_state = "00001")
                    then
                        if (done_encrypt = '0')
                        then
                            reset_encr <= '0';
                        elsif (done_encrypt = '1')
                        then
                            encr_grid_loc <= encr_out;
                            curr_state <= "00010";
                            reset_encr <= '1';
                            encr_in <= ack1;
                        end if;

                    elsif (curr_state = "00010")
                    then
                        if (done_encrypt = '0')
                        then
                            reset_encr <= '0';
                        elsif (done_encrypt = '1')
                        then
                            encr_ack1 <= encr_out;
                            curr_state <= "00011";
                            reset_encr <= '1';
                            encr_in <= ack2;
                        end if;

                    elsif (curr_state = "00011")
                    then
                        if (done_encrypt = '0')
                        then
                            reset_encr <= '0';
                        elsif (done_encrypt = '1')
                        then
                            encr_ack2 <= encr_out;
                            curr_state <= "00100";
                        end if;

                    elsif( curr_state = "00100")
                    then
                        --led_val <= "000" & curr_state;
                        if (pos2 = "000")
                        then
                    --            led_val <= encr_grid_loc(7 downto 0);
                                send_bits <= encr_grid_loc((31) downto (24));
                                pos2 <= pos2 + 1;
                        elsif (
                        f2hReady_in = '1' and 
                        chanAddr_in = "0000010" and
                                (pos2 < "100"))
                        then
                            --led_val <= encr_grid_loc((31 - 8*to_integer(pos2)) downto (24 - 8*to_integer(pos2)));
                            send_bits <= encr_grid_loc((31 - 8*to_integer(pos2)) downto (24 - 8*to_integer(pos2)));
                            pos2 <= pos2 + 1;
                        elsif (pos2 = "100")
                        then
                            --led_val <= encr_grid_loc(31 downto 24);
                            curr_state <= "00101";
                            counter <= (others => '0');
                            pos2 <= "000";
                        end if;

                    elsif (curr_state = "00101" and counter = x"2DC6C0000")
                    then
                    --    led_val <= "100"&curr_state;
                    --    counter <= (others => '0');
                        curr_state <= "10001";
                        counter <= x"044AA2001";
                        send_bits <= x"00";
                        pos2 <= "000";
                 
                    elsif (curr_state = "00101" and
                    change = '1' 
                    and (pos2 < "100"))
                    then
                    --    led_val <= reg0_next;
                        counter <= counter + 1;
                        recvdData((31 - 8*to_integer(pos2)) downto (24 - 8*to_integer(pos2))) <= reg0_next;
                        pos2 <= pos2 + 1;
                    elsif (curr_state = "00101" and pos2 = "100" )
                    then
                    --    led_val <= "100"&curr_state;
                        pos2 <= "000";
                        curr_state <= "00110";
                        reset_decr <= '1';
                        decr_in <= recvdData;
                        counter <= (others => '0');
        
                    elsif (curr_state = "00101" and (not change = '1'))
                    then
    --                            led_val <= "100"&curr_state;
                        counter <= counter + 1;
                    
                    elsif (curr_state = "00110" and done_decrypt = '0')
                    then
                        counter <= counter + 1;
                        reset_decr <= '0';
                    elsif (curr_state = "00110" and done_decrypt = '1')
                    then
                    --    led_val <= decr_out(7 downto 0);
                        pos2 <= "000";
                        if (decr_out = grid_loc)
                        then
                            curr_state <= "00111";
                            counter <= (others => '0');
                        else
--                            counter <= (others => '0');
--                            curr_state <= "00100";
                        curr_state <= "10001";
                        counter <= x"044AA2001";
                        send_bits <= x"00";
                        --    pos2 <= "000";
                        end if;

                    elsif (curr_state = "00111" and counter = x"2DC6C0000")
                    then
                    --    led_val <= "000"&curr_state;
--                        counter <= (others => '0');
--                        curr_state <= "00100";
                        curr_state <= "10001";
                        counter <= x"044AA2001";
                        send_bits <= x"00";
                        pos2 <= "000";
                    elsif (curr_state = "00111")
                    then
    --                    led_val <= "100"&curr_state;
                        if (pos2 = "000")
                        then
                      send_bits <= encr_ack1((31) downto (24));
                            counter <= counter + 1;
                            pos2 <= pos2 + 1;
                        elsif (f2hReady_in = '1' and chanAddr_in = "0000010" and
                                (pos2 < "100"))
                        then
                            counter <= counter + 1;
                            send_bits <= encr_ack1((31 - 8*to_integer(pos2)) downto (24 - 8*to_integer(pos2)));
                            pos2 <= pos2 + 1;
                        elsif (pos2 = "100")
                        then
                            curr_state <= "01000";
                            counter <= (others => '0');
                            pos2 <= "000";
                        else
                            counter <= counter + 1;
                        end if;


                    elsif (curr_state = "01000" and counter = x"2DC6C0000")
                    then
--                        counter <= (others => '0');
--                        curr_state <= "00100";
                        curr_state <= "10001";
                        counter <= x"044AA2001";
                        send_bits <= x"00";
                        pos2 <= "000";
                    elsif (curr_state = "01000" and change = '1' and (pos2 < "100"))
                    then
                        counter <= counter + 1;
                        recvdData((31 - 8*to_integer(pos2)) downto (24 - 8*to_integer(pos2))) <= reg0_next;
                        pos2 <= pos2 + 1;
                    elsif (curr_state = "01000" and pos2 = "100" )
                    then
                        pos2 <= "000";
                        counter <= (others => '0');
                        if (recvdData = encr_ack2)
                        then
                            curr_state <= "01001";
                        else
--                            curr_state <= "00100";
                        curr_state <= "10001";
                        counter <= x"044AA2001";
                        send_bits <= x"00";
                              end if;

                        elsif (curr_state = "01000" and (not change = '1'))
                    then
                        counter <= counter + 1;
                    
                    
                    
                    elsif (curr_state = "01001" and counter = x"2DC6C0000")
                    then
--                        counter <= (others => '0');
--                        curr_state <= "00100";
                        curr_state <= "10001";
                        counter <= x"044AA2001";
                        send_bits <= x"00";
                              pos2 <= "000";
                    elsif (curr_state = "01001" and change = '1' and (pos2 < "100"))
                    then
                        counter <= counter + 1;
                        temp_ans((63 - 8*to_integer(pos2)) downto (56 - 8*to_integer(pos2))) <= reg0_next;
                        pos2 <= pos2 + 1;
                    elsif (curr_state = "01001" and pos2 = "100" )
                    then
                        curr_state <= "01010";
                        reset_decr <= '1';
                        decr_in <= temp_ans(63 downto 32);
                        counter <= (others => '0');
                    elsif (curr_state = "01001" and (not change = '1'))
                    then
                        counter <= counter + 1;
                    
                    
                    
                    elsif (curr_state = "01010" and done_decrypt = '0')
                    then
                        counter <= counter + 1;
                        reset_decr <= '0';
                    elsif (curr_state = "01010" and done_decrypt = '1')
                    then
                        pos2 <= "000";
                        ans(63 downto 32) <= decr_out;
                        counter <= (others => '0');
                        curr_state <= "01011";

                    elsif (curr_state = "01011" and counter = x"2DC6C0000")
                    then
--                        counter <= (others => '0');
--                        curr_state <= "00100";
                 curr_state <= "10001";
                        counter <= x"044AA2001";
                        send_bits <= x"00";
                     pos2 <= "000";
                    elsif (curr_state = "01011")
                    then
                        if (pos2 = "000")
                        then
                            counter <= counter + 1;
                            send_bits <= encr_ack1((31) downto (24));
                                pos2 <= pos2 + 1;
                        elsif (f2hReady_in = '1' and chanAddr_in = "0000010" and
                                (pos2 < "100"))
                        then
                            counter <= counter + 1;
                            send_bits <= encr_ack1((31 - 8*to_integer(pos2)) downto (24 - 8*to_integer(pos2)));
                            pos2 <= pos2 + 1;
                        elsif (pos2 = "100")
                        then
                            curr_state <= "01101";
                            counter <= (others => '0');
                            pos2 <= "000";
                        else
                            counter <= counter + 1;
                        end if;



                    elsif (curr_state = "01101" and counter = x"2DC6C0000")
                    then
--                        counter <= (others => '0');
--                        curr_state <= "00100";
                        curr_state <= "10001";
                        counter <= x"044AA2001";
                        send_bits <= x"00";
                              pos2 <= "000";
                     elsif (curr_state = "01101" and change = '1' and (pos2 < "100"))
                    then
                        counter <= counter + 1;
                        temp_ans((31 - 8*to_integer(pos2)) downto (24 - 8*to_integer(pos2))) <= reg0_next;
                        pos2 <= pos2 + 1;
                    elsif (curr_state = "01101" and pos2 = "100" )
                    then
                        pos2 <= "000";
                        curr_state <= "01110";
                        reset_decr <= '1';
                        decr_in <= temp_ans(31 downto 0);
                        counter <= (others => '0');

                    elsif (curr_state = "01101" and (not change = '1'))
                    then
                        counter <= counter + 1;


                    elsif (curr_state = "01110" and done_decrypt = '0')
                    then
                        counter <= counter + 1;
                        reset_decr <= '0';
                    elsif (curr_state = "01110" and done_decrypt = '1')
                    then
                        ans(31 downto 0) <= decr_out;
                        counter <= (others => '0');
                        curr_state <= "01111";
                        pos2 <= "000";

                    elsif (curr_state = "01111" and counter = x"2DC6C0000")
                    then
--                        counter <= (others => '0');
--                        curr_state <= "00100";
                        curr_state <= "10001";
                        counter <= x"044AA2001";
                        send_bits <= x"00";
                              pos2 <= "000";
                    elsif (curr_state = "01111")
                    then
                        if (pos2 = "000")
                        then
                            counter <= counter + 1;
                            send_bits <= encr_ack1((31) downto (24));
                                    pos2 <= pos2 + 1;
                        elsif (f2hReady_in = '1' and chanAddr_in = "0000010" and
                                (pos2 < "100"))
                        then
                            counter <= counter + 1;
                            send_bits <= encr_ack1((31 - 8*to_integer(pos2)) downto (24 - 8*to_integer(pos2)));
                            pos2 <= pos2 + 1;
                        elsif (pos2 = "100")
                        then
                            curr_state <= "10000";
                            counter <= (others => '0');
                            pos2 <= "000";
                        else
                            counter <= counter + 1;
                        end if;


                    elsif (curr_state = "10000" and counter = x"2DC6C0000")
                    then
                        pos2 <= "000";
--                        counter <= (others => '0');
--                        curr_state <= "00100";
                        curr_state <= "10001";
                        counter <= x"044AA2001";
                        send_bits <= x"00";
                      elsif (curr_state = "10000" and change = '1' and (pos2 < "100"))
                    then
                        counter <= counter + 1;
                        recvdData((31 - 8*to_integer(pos2)) downto (24 - 8*to_integer(pos2))) <= reg0_next;
                        pos2 <= pos2 + 1;
                    elsif (curr_state = "10000" and pos2 = "100" )
                    then
                        pos2 <= "000";
                        counter <= (others => '0');
                        if (recvdData = encr_ack2)
                        then
                             -- led_val <= x"12";
                            send_bits <= x"00";
                            curr_state <= "10010";
                        else
                            --curr_state <= "00100";
                                    curr_state <= "10001";
                        counter <= x"044AA2001";
                        send_bits <= x"00";
                        end if;

                    elsif (curr_state = "10000" and (not change = '1'))
                    then
                        counter <= counter + 1;


                    elsif (curr_state = "10010" and pos2 < "111")
                    then
                        if (uart_ans(63 - 8*to_integer(pos2)) = '1')
                        then
                                if (ans(62 - 8*to_integer(pos2)) = '1') then
                                    ans(62 - 8*to_integer(pos2)) <= uart_ans(62 - 8*to_integer(pos2));
                                end if;
                            ans((58 - 8*to_integer(pos2)) downto (56 - 8*to_integer(pos2))) <= 
                            uart_ans((58 - 8*to_integer(pos2)) downto (56 - 8*to_integer(pos2)));
                        end if;
                        pos2 <= pos2 + 1;

                    elsif (curr_state = "10010" and pos2 = "111")
                    then
                        if (uart_ans(63 - 8*to_integer(pos2)) = '1')
                        then
                            if (ans(62 - 8*to_integer(pos2)) = '1') then
                                    ans(62 - 8*to_integer(pos2)) <= uart_ans(62 - 8*to_integer(pos2));
                                end if;
                            ans((58 - 8*to_integer(pos2)) downto (56 - 8*to_integer(pos2))) <= 
                            uart_ans((58 - 8*to_integer(pos2)) downto (56 - 8*to_integer(pos2)));
                        end if;
                        pos2 <= "000";
                            send_bits <= x"01";
                            --led_val <= x"13";
                        curr_state <= "10001";


                      elsif (curr_state = "10001")
                    then
                             
                        if(counter >= x"000000000" and counter <= x"008954400") then
                            counter <= counter + 1;
                            if(ans(63 downto 62) = "11" and sw_in(0) = '1') then
                                if (ans(31 downto 30) = "11" and sw_in(4) = '1') then
                                    led_val <= "00000001";
                                elsif (ans(58 downto 56) = "001") then
                                    led_val <= "00000010";
                                else
                                    led_val <= "00000100";
                                end if;
                            else
                                led_val <= "00000001";
--                                        led_val <= ans(63 downto 56);
                            end if;
                        elsif(counter > x"008954400" and counter <= x"0112A8800") then
                            counter <= counter + 1;
                            if(ans(55 downto 54) = "11" and sw_in(1) = '1') then
                                if (ans(23 downto 22) = "11" and sw_in(5) = '1') then
                                    led_val <= "00100001";
                                elsif (ans(50 downto 48) = "001") then
                                    led_val <= "00100010";
                                else
                                    led_val <= "00100100";
                                end if;
                            else
                                led_val <= "00100001";
--                                         led_val <= ans(55 downto 48);
                            end if;
                        elsif(counter > x"0112A8800" and counter <= x"019BFCC00") then
                            counter <= counter + 1;
                            if(ans(47 downto 46) = "11" and sw_in(2) = '1') then
                                if (ans(15 downto 14) = "11" and sw_in(6) = '1') then
                                    led_val <= "01000001";
                                elsif (ans(42 downto 40) = "001") then
                                    led_val <= "01000010";
                                else
                                    led_val <= "01000100";
                                end if;
                            else
                                led_val <= "01000001";
--                                        led_val <= ans(47 downto 40);
                            end if;
                        elsif(counter > x"019BFCC00" and counter <= x"022551000") then
                            counter <= counter + 1;
                            if(ans(39 downto 38) = "11" and sw_in(3) = '1') then
                                if (ans(7 downto 6) = "11" and sw_in(7) = '1') then
                                    led_val <= "01100001";
                                elsif (ans(34 downto 32) = "001") then
                                    led_val <= "01100010";
                                else
                                    led_val <= "01100100";
                                end if;
                            else
                                led_val <= "01100001";
--                                      led_val <= ans(39 downto 32);
                            end if;
                        elsif(counter > x"022551000" and counter <= x"02AEA5400") then
                            counter <= counter + 1;
                            if(ans(31 downto 30) = "11" and sw_in(4) = '1') then
                                if (ans(63 downto 62) = "11" and sw_in(0) = '1') then
                                    if (counter > x"022551000" and counter <= x"025317C00") then
                                        led_val <= "10000100";
                                    elsif (counter > x"025317C00" and counter <= x"0280DE800") then
                                        led_val <= "10000010";
                                    else
                                        led_val <= "10000001";
                                    end if;
                                elsif (ans(26 downto 24) = "001") then
                                    led_val <= "10000010";
                                else
                                    led_val <= "10000100";
                                end if;
                            else
                                led_val <= "10000001";
                            end if;
                        elsif(counter > x"02AEA5400" and counter <= x"0337F9800") then
                            counter <= counter + 1;
                            if(ans(23 downto 22) = "11" and sw_in(5) = '1') then
                                if (ans(55 downto 54) = "11" and sw_in(1) = '1') then
                                    if (counter > x"02AEA5400" and counter <= x"02DC6C000") then
                                        led_val <= "10100100";
                                    elsif (counter > x"02DC6C000" and counter <= x"030A32C00") then
                                        led_val <= "10100010";
                                    else
                                        led_val <= "10100001";
                                    end if;
                                elsif (ans(18 downto 16) = "001") then
                                    led_val <= "10100010";
                                else
                                    led_val <= "10100100";
                                end if;
                            else
                                led_val <= "10100001";
                            end if;
                        elsif(counter > x"0337F9800" and counter <= x"03C14DC00") then
                            counter <= counter + 1;
                            if(ans(15 downto 14) = "11" and sw_in(6) = '1') then
                                if (ans(47 downto 46) = "11" and sw_in(2) = '1') then
                                    if (counter > x"0337F9800" and counter <= x"0365C0400") then
                                        led_val <= "11000100";
                                    elsif (counter > x"0365C0400" and counter <= x"039387000") then
                                        led_val <= "11000010";
                                    else
                                        led_val <= "11000001";
                                    end if;
                                elsif (ans(10 downto 8) = "001") then
                                    led_val <= "11000010";
                                else
                                    led_val <= "11000100";
                                end if;
                            else
                                led_val <= "11000001";
                            end if;
                        elsif(counter > x"03C14DC00" and counter <= x"044AA2000") then
                            counter <= counter + 1;
                            if(ans(7 downto 6) = "11" and sw_in(7) = '1') then
                                if (ans(39 downto 38) = "11" and sw_in(3) = '1') then
                                    if (counter > x"03C14DC00" and counter <= x"03EF14800") then
                                        led_val <= "11100100";
                                    elsif (counter > x"03EF14800" and counter <= x"041CDB400") then
                                        led_val <= "11100010";
                                    else
                                        led_val <= "11100001";
                                    end if;
                                elsif (ans(2 downto 0) = "001") then
                                    led_val <= "11100010";
                                else
                                    led_val <= "11100100";
                                end if;
                            else
                                led_val <= "11100001";
                            end if;

                        elsif (counter = x"044AA2002" and connection_counter = x"08954400")
                        then 

                            curr_state <= "00000";
                            connection_counter <= x"00000000";
                            counter <= (others => '0');
                            send_bits <= x"00";
                            if(send_bits = x"08")
                            then 
                                macro_state <= "100";
                            elsif(send_bits = x"a8")
                            then
                                macro_state <= "010";
                            elsif(send_bits = x"c4")
                            then 
                                macro_state <= "011";
                            end if;

                        elsif (f2hReady_in = '1' and chanAddr_in = "0000010" and send_bits = x"a8" and counter = x"044AA2002")
                        then
                            curr_state <= "00000";
                            macro_state <= "010";
                            counter <= (others => '0');
                            send_bits <= x"00";
                        elsif(counter = x"044AA2002" and ub = '1') 
                        then
                            send_bits <= x"a8";

                        elsif (f2hReady_in = '1' and chanAddr_in = "0000010" and send_bits = x"c4" and counter = x"044AA2002")
                        then
                            curr_state <= "00000";
                            macro_state <= "011";
                            counter <= (others => '0');

                        elsif(counter = x"044AA2002" and lb = '1' and send_bits = x"01") 
                        then
                            send_bits <= x"c4";

                        elsif(f2hReady_in = '1' and chanAddr_in = "0000010" and counter = x"044AA2002" and send_bits = x"08") 
                        then
                            curr_state <= "00000";
                            macro_state <= "100";
                            counter <= (others => '0');
                            send_bits <= x"00";
                        elsif(counter = x"044AA2002" and send_bits = x"01") 
                        then
                            send_bits <= x"08";
                        elsif(counter = x"044AA2001")
                        then
                            counter <= counter + 1; 
                            led_val <= "00000000";
                        end if;
                        elsif(counter = x"044AA2002" and connection_counter < x"08954400")
                        then
                            connection_counter <= connection_counter + 1;
                    end if;

                elsif (macro_state = "010")
                then
                    if (curr_state = "00000" and db = '1')
                    then
                        curr_state <= "10000";
                    elsif (curr_state = "10000")
                    then
                        reset_encr <= '1';
                        encr_in <= "000000000000000000000000"&sw_in;
                        curr_state <= "10001";
                 --    led_val <= "00000000";

                    elsif (curr_state = "10001")
                    then
                        if (done_encrypt = '0')
                        then
                                --    led_val <= x"f5";
                            reset_encr <= '0';
                        elsif (done_encrypt = '1')
                        then
                                --    led_val <= x"f3";
                            encr_sw_in <= encr_out;
                            curr_state <= "00001";
                            reset_encr <= '1';
                            connection_counter <= x"00000000";
                            send_bits <= x"bb";
                        end if;

                    elsif(curr_state = "00001")
                    then
                        if(f2hReady_in = '1' and chanAddr_in = "0000010")
                        then    
                            curr_state <= "00010";
                            pos2 <= "001";
                            send_bits <= encr_sw_in((31) downto (24));
                            connection_counter <= x"00000000";
                        elsif(connection_counter < x"08954400")
                        then
                            connection_counter <= connection_counter + 1;
                        else
                            curr_state <= "00010";
                            pos2 <= "001";
                            connection_counter <= x"00000000";
                            if(lb = '1')
                            then
                                curr_state <= "00000";
                                macro_state <= "011";
                                send_bits <= x"00";
                            else
                                curr_state <= "00000";
                                macro_state <= "100";
                                send_bits <= x"00";
                            end if;
                        end if;
                    --    led_val <= x"fd";
                        
                    elsif(curr_state = "00010")
                    then
                        if (f2hReady_in = '1' and chanAddr_in = "0000010" and (pos2 < "100"))
                        then
                        --    led_val <= x"56";

                            send_bits <= encr_sw_in((31 - 8*to_integer(pos2)) downto (24 - 8*to_integer(pos2)));
                            pos2 <= pos2 + 1;
                        elsif (pos2 = "100")
                        then
                        --    led_val <= encr_sw_in((31) downto (24));
                            counter <= (others => '0');
                            pos2 <= "000";
                            send_bits <= x"00";
                        curr_state <= "00011";
                    end if;
                        
--                    elsif (f2hReady_in = '1' and chanAddr_in = "0000010" and curr_state = "00001")
--                    then
--                        send_bits <= sw_in;
--                        curr_state <= "00010";
--                    elsif (curr_state = "00010" and f2hReady_in = '1' and chanAddr_in = "0000010")
--                    then
--                        send_bits <= x"00";
--                        curr_state <= "00011";
--                    
                    elsif (f2hReady_in = '1' and chanAddr_in = "0000010" and send_bits = x"c4" and curr_state = "00011")
                    then
                        curr_state <= "00000";
                        macro_state <= "011";
                        send_bits <= x"00";
                    elsif (curr_state = "00011" and lb = '1')
                    then
                        send_bits <= x"c4";
                    elsif (f2hReady_in = '1' and chanAddr_in = "0000010" and send_bits = x"08" and curr_state = "00100")
                    then
                        curr_state <= "00000";
                        macro_state <= "100";
                        send_bits <= x"00";
                    elsif (curr_state = "00011" and send_bits = x"00")
                    then
                        send_bits <= x"08";
                        curr_state <= "00100";
                    end if;
                        
                        

                elsif (macro_state = "011")
                then
                    if (curr_state = "00000" and rb = '1')
                    then
                        uart_tx_data <= sw_in;
                        curr_state <= "00001";
                        elsif (curr_state = "00000")
                        then
                        --    led_val <= x"88";
                    elsif (curr_state = "00001")
                    then
                        curr_state <= "00010";
                    elsif (curr_state = "00010" and uart_tx_counter < "1111")
                    then
                        uart_tx_counter <= uart_tx_counter + 1;
                    elsif (curr_state = "00010" and uart_tx_counter = "1111")
                    then
                        curr_state <= "00011";
                            
                            txdata <= uart_tx_data;
                        uart_tx_counter <= "0000";
                    elsif (curr_state = "00011")
                    then
                        tx_start <= '1';
                        curr_state <= "00100";
                    elsif (curr_state = "00100" and txdone = '1')
                    then
                        uart_tx_data <= x"00";
                            tx_start <= '0';
                        send_bits <= x"00";
                            curr_state <= "00000";
                            macro_state <= "100";
                        end if;
                

                elsif (macro_state = "100")
                then
                    if(curr_state = "00000" and uart_change = '1')
                    then
                    --    led_val <= uart_rx_data;
                        curr_state <= "00001";
                    
                        
                    
                    elsif(curr_state = "00001")
                    then 
                        uart_ans((63-8*to_integer(unsigned(uart_rx_data(7 downto 5)))) downto 
                            (62-8*to_integer(unsigned(uart_rx_data(7 downto 5)))))
                            <= uart_rx_data(4 downto 3);
                        uart_ans((61-8*to_integer(unsigned(uart_rx_data(7 downto 5)))) downto 
                            (59-8*to_integer(unsigned(uart_rx_data(7 downto 5)))))
                            <= uart_rx_data(7 downto 5);
                        uart_ans((58-8*to_integer(unsigned(uart_rx_data(7 downto 5)))) downto 
                            (56-8*to_integer(unsigned(uart_rx_data(7 downto 5)))))
                            <= uart_rx_data(2 downto 0);
                        
                        curr_state <= "00011";
                            
                    else 
                        macro_state <= "101";
                        curr_state <= "00000";
--                        uart_rx_data <= "00000000";
--                        ANY OTHER RESETTINGS
                    end if;
                elsif (macro_state = "101")
                then
                    if(s6_counter < x"008954400")
                    then
                        s6_counter <= s6_counter + 1;
                    else 
                        s6_counter <= (others => '0');
                        macro_state <= "001";
                        curr_state <= "00000";
                        temp_ans <= (others => '0');
                         encr_in <= (others => '0');
                        recvdData <= (others => '0');
                        pos2 <= "000";
                        counter <= x"000000000";
--                        curr_state <= "00000";
--                        macro_state <= "000";
                        uart_tx_counter <= "0000";

                    end if;
                end if;
            end if; 
    end process;

    -- Drive register inputs for each channel when the host is writing
    reg0_next <=
        h2fData_in when (h2fValid_in = '1')
        else reg0_next;

    change <= '1' when (chanAddr_in = "0000011" and h2fValid_in = '1')
        else '0';

    with chanAddr_in select f2hData_out <=
        send_bits when "0000010",
        x"00" when others;
    -- Assert that there's always data for reading, and always room for writing
    h2fReady_out <= '1';
    f2hValid_out <= '1';--END_SNIPPET(registers)

    -- LEDs and 7-seg display
    led_out <= led_val;

--   flags <= "00" & f2hReady_in & reset_in;
    
    data_cap : process (clk_in, rxdone, deb_res, macro_state)
    begin
        if (deb_res = '1')
        then
            uart_change <= '0';
            uart_rx_data <= "00000000";
        elsif (rising_edge(clk_in))
        then
            if(rxdone = '1') then
                uart_change <= '1';
                uart_rx_data <= rxdata;
            elsif (macro_state = "000")
            then
                uart_change <= '0';
            end if;
        end if;
    
    end process;
    
end architecture;