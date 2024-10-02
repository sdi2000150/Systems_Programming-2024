./multijob.sh tests/commands_3.txt
./jobCommander issueJob setConcurrency 4
./multijob.sh tests/commands_4.txt
./allJobsStop.sh
./jobCommander  poll running
./jobCommander  poll queued
ps -aux | grep progDelay
