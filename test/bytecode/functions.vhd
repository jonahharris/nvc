package functions is
end package;

package body functions is

    function fact(n : integer) return integer is
        variable result : integer := 1;
    begin
        for i in 1 to n loop
            result := result * i;
        end loop;
        return result;
    end function;

end package body;
