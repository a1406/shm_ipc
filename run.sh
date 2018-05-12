#!/bin/bash
NUM=1000000000
#rm nohup
./test 0 1 1
cmd="./test ${NUM} 0 1 "
for k in $( seq 1 5 )
do
    cmd=${cmd}"& ./test ${NUM} 0 1 "
done
echo ${cmd}
`${cmd}`
#./test ${NUM} 0 1 & ./test ${NUM} 0 1 & ./test ${NUM} 0 1 & ./test ${NUM} 0 1 & ./test ${NUM} 0 1 & ./test ${NUM} 0 1
