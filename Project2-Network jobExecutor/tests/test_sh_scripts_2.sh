./tests/multijob.sh tests/commands_3.txt
./bin/jobCommander localhost 8881 issueJob setConcurrency 4
./tests/multijob.sh tests/commands_4.txt
./tests/allJobsStop.sh
./bin/jobCommander localhost 8881 poll
ps -aux | grep progDelay
