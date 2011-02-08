#!/usr/bin/env katana
$e=load "test"
dwarfscript emit ".eh_frame" $e "test.dws"
#just compile back the same dwarfscript to make sure we can
$ehframe=dwarfscript compile "test.dws"
replace section $e ".eh_frame" $ehframe[0]
replace section $e ".eh_frame_hdr" $ehframe[1]
replace section $e ".gcc_except_table" $ehframe[2]
save $e "test_mod"
!chmod +x "test_mod"
$f=load "test_mod"
dwarfscript emit ".eh_frame" $f "test_mod.dws"
