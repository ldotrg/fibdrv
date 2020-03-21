#!/bin/bash
TOP_DIR=$(pwd)
echo $TOP_DIR
# STOP_DIR=$(echo $TOP_DIR | sed 's/\//\\\//g')
# echo $STOP_DIR
# sed -i -e 's/SED_REPLACE_PATH/'$STOP_DIR'/g' users_probe.bt

echo "Data will log into the performance.csv... Ctrl+C to stop !!"

sudo bpftrace -e '
BEGIN 
    { 
        @start = nsecs; 
    }
uprobe:'$TOP_DIR'/client:debug_read /@start != 0/ 
    { 
        @start = nsecs; 
    }
uretprobe:'$TOP_DIR'/client:debug_read 
    { 
      @cnt++; @stop = nsecs; 
      printf("%d ,%llu\n", (@cnt - 1), (@stop - @start)); 
    }
END 
    { 
        clear(@start); clear(@stop); clear(@cnt);
    }' > performance.csv


#sudo bpftrace users_probe.bt