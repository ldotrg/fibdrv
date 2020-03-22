#!/usr/bin/env bash
# TOP_DIR=$(pwd)
# echo $TOP_DIR
# STOP_DIR=$(echo $TOP_DIR | sed 's/\//\\\//g')
# echo $STOP_DIR
# sed -i -e 's/SED_REPLACE_PATH/'$STOP_DIR'/g' users_probe.bt
#sudo bpftrace users_probe.bt

clean_up() 
{
    # Perform program exit housekeeping
    sed -i 's/Attaching 4 probes...//g' performance.csv
    exit
}
trap clean_up SIGHUP SIGINT SIGTERM
echo "Data will preserved in performance.csv... Press Ctrl+C to stop."

sudo bpftrace -e '
BEGIN 
    { 
        @start = nsecs;
    }
uprobe:'$PWD'/client:debug_read /@start != 0/ 
    { 
        @start = nsecs; 
    }
uretprobe:'$PWD'/client:debug_read 
    { 
      @cnt[pid]++; @stop = nsecs; 
      printf("%d ,%llu\n", (@cnt[pid] - 1), (@stop - @start)); 
    }
END 
    { 
        clear(@start); clear(@stop); clear(@cnt);
    }' | tee performance.csv 

